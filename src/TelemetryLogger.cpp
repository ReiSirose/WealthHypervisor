#include "TelemetryLogger.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>

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