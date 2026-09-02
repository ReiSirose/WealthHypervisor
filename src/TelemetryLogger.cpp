#include "TelemetryLogger.hpp"

#include "mio.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <system_error>

void TelemetryLogger::reserve_years(size_t expected_years) {
    history.reserve(expected_years);
}

void TelemetryLogger::record_year(AnnualSnapshot snapshot) {
    history.push_back(std::move(snapshot));
}

void TelemetryLogger::clear() noexcept {
    history.clear();
}

bool TelemetryLogger::export_to_json(const std::string& filepath) const {
    std::ofstream file(filepath, std::ios::out | std::ios::trunc);

    if (!file.is_open()) {
        std::cerr << "[TelemetryLogger] Error: Unable to open output file: " << filepath << "\n";
        return false;
    }

    file << std::fixed << std::setprecision(2);

    file << "{\n";
    file << "  \"simulation_summary\": {\n";
    file << "    \"total_years\": " << history.size() << ",\n";
    file << "    \"starting_aum\": " << (history.empty() ? 0.0 : history.front().starting_aum) << ",\n";
    file << "    \"final_aum\": " << (history.empty() ? 0.0 : history.back().ending_aum) << "\n";
    file << "  },\n";
    file << "  \"annual_records\": [\n";

    for (size_t i = 0; i < history.size(); ++i) {
        // Annual snapshot
        const auto& snap = history[i];
        file << "    {\n";
        file << "      \"year\": " << snap.year << ",\n";
        file << "      \"starting_aum\": " << snap.starting_aum << ",\n";
        file << "      \"ending_aum\": " << snap.ending_aum << ",\n";
        file << "      \"annual_market_return\": " << std::setprecision(6) << snap.annual_market_return << std::setprecision(2) << ",\n";
        file << "      \"global_cap_dollars\": " << snap.global_cap_dollars << ",\n";
        file << "      \"total_base_disbursed\": " << snap.total_base_disbursed << ",\n";
        file << "      \"total_spillover_disbursed\": " << snap.total_spillover_disbursed << ",\n";
        file << "      \"unused_surplus_retained\": " << snap.unused_surplus_retained << ",\n";

        // --- BRANCH SNAPSHOTS ---
        file << "      \"branches\": [\n";
        for (size_t b = 0; b < snap.branch_states.size(); ++b) {
            const auto& br = snap.branch_states[b];
            file << "        {\n";
            file << "          \"branch_id\": " << br.branch_id << ",\n";
            file << "          \"parent_index\": " << br.parent_index << ",\n";
            file << "          \"share_percentage\": " << std::setprecision(4) << br.virtual_share_percentage << std::setprecision(2) << ",\n";
            file << "          \"base_cap_dollars\": " << br.base_cap_dollars << ",\n";
            file << "          \"base_disbursed\": " << br.base_disbursed << ",\n";
            file << "          \"spillover_disbursed\": " << br.spillover_disbursed << ",\n";
            file << "          \"active_heir_count\": " << br.active_heir_count << "\n";
            file << "        }" << (b + 1 < snap.branch_states.size() ? "," : "") << "\n";
        }
        file << "      ],\n";

        // --- HEIR SNAPSHOTS ---
        file << "      \"heirs\": [\n";
        for (size_t h = 0; h < snap.heir_states.size(); ++h) {
            const auto& heir = snap.heir_states[h];
            file << "        {\n";
            file << "          \"heir_id\": " << heir.heir_id << ",\n";
            file << "          \"branch_id\": " << heir.branch_id << ",\n";
            file << "          \"age\": " << heir.age << ",\n";
            file << "          \"capital_contribution\": " << heir.capital_contribution << ",\n";
            file << "          \"raw_match_demand\": " << heir.raw_match_demand << ",\n";
            file << "          \"base_payout\": " << heir.base_payout << ",\n";
            file << "          \"spillover_payout\": " << heir.spillover_payout << ",\n";
            file << "          \"total_payout\": " << heir.total_payout() << ",\n";
            file << "          \"unmet_demand\": " << heir.unmet_demand << "\n";
            file << "        }" << (h + 1 < snap.heir_states.size() ? "," : "") << "\n";
        }
        file << "      ]\n";

        file << "    }" << (i + 1 < history.size() ? "," : "") << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    file.close();
    return true;
}

bool TelemetryLogger::export_to_binary_mmap(const std::string& filepath) const {
    constexpr std::uint32_t MAGIC = 0x544C4D42u;
    constexpr std::uint32_t VERSION = 1u;
    const std::uint32_t count = static_cast<std::uint32_t>(history.size());

    std::size_t total_bytes = 3u * sizeof(std::uint32_t);
    for (const auto& snapshot : history) {
        total_bytes += sizeof(std::uint32_t); // year
        total_bytes += 8u * sizeof(double);

        total_bytes += sizeof(std::uint32_t); // branch_count
        total_bytes += snapshot.branch_states.size() * (
            2u * sizeof(std::uint32_t) +
            4u * sizeof(double) +
            sizeof(std::uint32_t));

        total_bytes += sizeof(std::uint32_t); // heir_count
        total_bytes += snapshot.heir_states.size() * (
            sizeof(std::uint64_t) +
            2u * sizeof(std::uint32_t) +
            5u * sizeof(double));
    }

    std::ofstream file(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[TelemetryLogger] Error: unable to open file for mmap export: " << filepath << "\n";
        return false;
    }
    file.seekp(static_cast<std::streamoff>(total_bytes - 1));
    file.put('\0');
    file.close();

    std::error_code error;
    mio::mmap_sink sink = mio::make_mmap_sink(filepath, 0, mio::map_entire_file, error);
    if (error) {
        std::cerr << "[TelemetryLogger] Error: unable to map export file: " << error.message() << "\n";
        return false;
    }

    char* cursor = sink.data();
    auto put_u32 = [&](std::uint32_t value) {
        std::memcpy(cursor, &value, sizeof(value));
        cursor += sizeof(value);
    };
    auto put_u64 = [&](std::uint64_t value) {
        std::memcpy(cursor, &value, sizeof(value));
        cursor += sizeof(value);
    };
    auto put_double = [&](double value) {
        std::memcpy(cursor, &value, sizeof(value));
        cursor += sizeof(value);
    };
    auto put_branch = [&](const BranchSnapshot& branch) {
        put_u32(branch.branch_id);
        put_u32(branch.parent_index);
        put_double(branch.virtual_share_percentage);
        put_double(branch.base_cap_dollars);
        put_double(branch.base_disbursed);
        put_double(branch.spillover_disbursed);
        put_u32(static_cast<std::uint32_t>(branch.active_heir_count));
    };
    auto put_heir = [&](const HeirSnapshot& heir) {
        put_u64(heir.heir_id);
        put_u32(heir.branch_id);
        put_u32(static_cast<std::uint32_t>(heir.age));
        put_double(heir.capital_contribution);
        put_double(heir.raw_match_demand);
        put_double(heir.base_payout);
        put_double(heir.spillover_payout);
        put_double(heir.unmet_demand);
    };
    auto put_snapshot = [&](const AnnualSnapshot& snapshot) {
        put_u32(snapshot.year);
        put_double(snapshot.starting_aum);
        put_double(snapshot.ending_aum);
        put_double(snapshot.annual_market_return);
        put_double(snapshot.global_cap_dollars);
        put_double(snapshot.total_base_disbursed);
        put_double(snapshot.total_spillover_disbursed);
        put_double(snapshot.unused_surplus_retained);

        const std::uint32_t branch_count = static_cast<std::uint32_t>(snapshot.branch_states.size());
        put_u32(branch_count);
        for (const auto& branch : snapshot.branch_states) {
            put_branch(branch);
        }

        const std::uint32_t heir_count = static_cast<std::uint32_t>(snapshot.heir_states.size());
        put_u32(heir_count);
        for (const auto& heir : snapshot.heir_states) {
            put_heir(heir);
        }
    };

    put_u32(MAGIC);
    put_u32(VERSION);
    put_u32(count);

    for (const auto& snapshot : history) {
        put_snapshot(snapshot);
    }

    sink.sync(error);
    if (error) {
        std::cerr << "[TelemetryLogger] Error: unable to flush mmap export: " << error.message() << "\n";
        return false;
    }

    sink.unmap();
    return true;
}