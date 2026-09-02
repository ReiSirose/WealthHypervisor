#!/usr/bin/env python3
"""
Binary telemetry parser for the WealthHypervisor mmap export format.

File format:
    [MAGIC:u32] [VERSION:u32] [SNAPSHOT_COUNT:u32]
    repeated:
        [year:u32]
        [starting_aum:f64]
        [ending_aum:f64]
        [annual_market_return:f64]
        [global_cap_dollars:f64]
        [total_base_disbursed:f64]
        [total_spillover_disbursed:f64]
        [unused_surplus_retained:f64]
        [branch_count:u32]
        [branch_count * BranchSnapshot]
        [heir_count:u32]
        [heir_count * HeirSnapshot]

BranchSnapshot layout:
    [branch_id:u32]
    [parent_index:u32]
    [virtual_share_percentage:f64]
    [base_cap_dollars:f64]
    [base_disbursed:f64]
    [spillover_disbursed:f64]
    [active_heir_count:u32]

HeirSnapshot layout:
    [heir_id:u64]
    [branch_id:u32]
    [age:u32]
    [capital_contribution:f64]
    [raw_match_demand:f64]
    [base_payout:f64]
    [spillover_payout:f64]
    [unmet_demand:f64]
"""

import argparse
import struct
import sys
from dataclasses import dataclass
from typing import BinaryIO, List

import numpy as np

MAGIC_NUMBER = 0x544C4D42  # "TLMB"
VERSION = 1

BRANCH_SNAPSHOT_FMT = struct.Struct('<IIddddI')
HEIR_SNAPSHOT_FMT = struct.Struct('<QIIddddd')
# Exact layout written by TelemetryLogger::export_to_binary_mmap():
#   - branch snapshot: u32, u32, d, d, d, d, u32
#   - heir snapshot: u64, u32, u32, d, d, d, d, d

BRANCH_SNAPSHOT_SIZE = BRANCH_SNAPSHOT_FMT.size
HEIR_SNAPSHOT_SIZE = HEIR_SNAPSHOT_FMT.size


@dataclass
class HeirSnapshot:
    heir_id: int
    branch_id: int
    age: int
    capital_contribution: float
    raw_match_demand: float
    base_payout: float
    spillover_payout: float
    unmet_demand: float

    @property
    def total_payout(self) -> float:
        return self.base_payout + self.spillover_payout

    @property
    def effective_match_ratio(self) -> float:
        return (self.total_payout / self.capital_contribution) if self.capital_contribution > 0 else 0.0


@dataclass
class BranchSnapshot:
    branch_id: int
    parent_index: int
    virtual_share_percentage: float
    base_cap_dollars: float
    base_disbursed: float
    spillover_disbursed: float
    active_heir_count: int

    @property
    def total_disbursed(self) -> float:
        return self.base_disbursed + self.spillover_disbursed


@dataclass
class AnnualSnapshot:
    year: int
    starting_aum: float
    ending_aum: float
    annual_market_return: float
    global_cap_dollars: float
    total_base_disbursed: float
    total_spillover_disbursed: float
    unused_surplus_retained: float
    branch_states: List[BranchSnapshot]
    heir_states: List[HeirSnapshot]


class TelemetryBinaryReader:
    """Read the mmap-exported binary telemetry stream."""

    def __init__(self, filepath: str):
        self.filepath = filepath
        self.snapshots: List[AnnualSnapshot] = []
        self._read_file()

    def _read_file(self) -> None:
        with open(self.filepath, 'rb') as f:
            magic = struct.unpack('<I', f.read(4))[0]
            if magic != MAGIC_NUMBER:
                raise ValueError(f"Invalid magic number: 0x{magic:08x}, expected 0x{MAGIC_NUMBER:08x}")

            version = struct.unpack('<I', f.read(4))[0]
            if version != VERSION:
                raise ValueError(f"Unsupported version: {version}, expected {VERSION}")

            snapshot_count = struct.unpack('<I', f.read(4))[0]
            for _ in range(snapshot_count):
                self.snapshots.append(self._read_snapshot(f))

    def _read_snapshot(self, f: BinaryIO) -> AnnualSnapshot:
        year, starting_aum, ending_aum, annual_market_return, global_cap_dollars, total_base_disbursed, total_spillover_disbursed, unused_surplus_retained = struct.unpack(
            '<Iddddddd',
            f.read(4 + 7 * 8),
        )

        branch_count = struct.unpack('<I', f.read(4))[0]
        branch_states: List[BranchSnapshot] = []
        for _ in range(branch_count):
            branch_id, parent_index, virtual_share_percentage, base_cap_dollars, base_disbursed, spillover_disbursed, active_heir_count = BRANCH_SNAPSHOT_FMT.unpack(
                f.read(BRANCH_SNAPSHOT_SIZE)
            )
            branch_states.append(
                BranchSnapshot(
                    branch_id=branch_id,
                    parent_index=parent_index,
                    virtual_share_percentage=virtual_share_percentage,
                    base_cap_dollars=base_cap_dollars,
                    base_disbursed=base_disbursed,
                    spillover_disbursed=spillover_disbursed,
                    active_heir_count=active_heir_count,
                )
            )

        heir_count = struct.unpack('<I', f.read(4))[0]
        heir_states: List[HeirSnapshot] = []
        for _ in range(heir_count):
            heir_id, branch_id, age, capital_contribution, raw_match_demand, base_payout, spillover_payout, unmet_demand = HEIR_SNAPSHOT_FMT.unpack(
                f.read(HEIR_SNAPSHOT_SIZE)
            )
            heir_states.append(
                HeirSnapshot(
                    heir_id=heir_id,
                    branch_id=branch_id,
                    age=age,
                    capital_contribution=capital_contribution,
                    raw_match_demand=raw_match_demand,
                    base_payout=base_payout,
                    spillover_payout=spillover_payout,
                    unmet_demand=unmet_demand,
                )
            )

        return AnnualSnapshot(
            year=year,
            starting_aum=starting_aum,
            ending_aum=ending_aum,
            annual_market_return=annual_market_return,
            global_cap_dollars=global_cap_dollars,
            total_base_disbursed=total_base_disbursed,
            total_spillover_disbursed=total_spillover_disbursed,
            unused_surplus_retained=unused_surplus_retained,
            branch_states=branch_states,
            heir_states=heir_states,
        )

    def export_to_numpy(self) -> dict:
        years = np.array([s.year for s in self.snapshots], dtype=np.uint32)
        starting_aum = np.array([s.starting_aum for s in self.snapshots], dtype=np.float64)
        ending_aum = np.array([s.ending_aum for s in self.snapshots], dtype=np.float64)
        market_returns = np.array([s.annual_market_return for s in self.snapshots], dtype=np.float64)
        return {
            'years': years,
            'starting_aum': starting_aum,
            'ending_aum': ending_aum,
            'market_returns': market_returns,
            'total_disbursed': starting_aum - ending_aum + market_returns * starting_aum,
        }

    def get_snapshot(self, year: int) -> AnnualSnapshot | None:
        for snap in self.snapshots:
            if snap.year == year:
                return snap
        return None

    def summary(self) -> None:
        if not self.snapshots:
            print('No snapshots loaded.')
            return

        first = self.snapshots[0]
        last = self.snapshots[-1]
        print(f'Telemetry Summary ({self.filepath})')
        print(f'  Total Years: {len(self.snapshots)}')
        print(f'  Starting AUM: ${first.starting_aum:,.2f}')
        print(f'  Final AUM: ${last.ending_aum:,.2f}')
        print(f'  Total Growth: {((last.ending_aum / first.starting_aum) - 1) * 100:.2f}%')
        print(f'  Total Disbursed (Base): ${sum(s.total_base_disbursed for s in self.snapshots):,.2f}')
        print(f'  Total Disbursed (Spillover): ${sum(s.total_spillover_disbursed for s in self.snapshots):,.2f}')

    def print_sample(self, snapshot_index: int = 0, branch_limit: int = 3, heir_limit: int = 3) -> None:
        if not self.snapshots:
            print('No snapshots loaded.')
            return

        if snapshot_index < 0 or snapshot_index >= len(self.snapshots):
            raise ValueError(f'snapshot_index={snapshot_index} out of range for {len(self.snapshots)} snapshots')

        snap = self.snapshots[snapshot_index]
        print(f'\nSample snapshot (year {snap.year})')

        print('  Branches:')
        for i, branch in enumerate(snap.branch_states[:branch_limit]):
            print(
                f'    [{i}] branch_id={branch.branch_id}, parent_index={branch.parent_index}, '
                f'virtual_share={branch.virtual_share_percentage:.4f}, '
                f'base_cap=${branch.base_cap_dollars:,.2f}, '
                f'base_disbursed=${branch.base_disbursed:,.2f}, '
                f'spillover_disbursed=${branch.spillover_disbursed:,.2f}, '
                f'active_heirs={branch.active_heir_count}'
            )

        print('  Heirs:')
        for i, heir in enumerate(snap.heir_states[:heir_limit]):
            print(
                f'    [{i}] heir_id={heir.heir_id}, branch_id={heir.branch_id}, age={heir.age}, '
                f'capital=${heir.capital_contribution:,.2f}, '
                f'raw_match_demand=${heir.raw_match_demand:,.2f}, '
                f'base_payout=${heir.base_payout:,.2f}, '
                f'spillover_payout=${heir.spillover_payout:,.2f}, '
                f'unmet_demand=${heir.unmet_demand:,.2f}'
            )


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Parse the WealthHypervisor binary telemetry export.')
    parser.add_argument('telemetry_bin', help='Path to the .bin file generated by the simulation export')
    parser.add_argument('--snapshot-index', type=int, default=0, help='Which annual snapshot to sample (default: 0)')
    parser.add_argument('--branch-limit', type=int, default=3, help='How many branch entries to print (default: 3)')
    parser.add_argument('--heir-limit', type=int, default=3, help='How many heir entries to print (default: 3)')
    args = parser.parse_args()

    reader = TelemetryBinaryReader(args.telemetry_bin)
    reader.summary()
    reader.print_sample(
        snapshot_index=args.snapshot_index,
        branch_limit=args.branch_limit,
        heir_limit=args.heir_limit,
    )
