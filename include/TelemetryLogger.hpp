#pragma once

#include "TelemetryTypes.hpp"
#include <vector>
#include <string>
#include <cstddef>

class TelemetryLogger {
private:
    std::vector<AnnualSnapshot> history;

public:
    TelemetryLogger() = default;

    void reserve_years(size_t expected_years);
    void record_year(AnnualSnapshot snapshot);

    [[nodiscard]] bool export_to_json(const std::string& filepath) const;
    void clear() noexcept;

    [[nodiscard]] size_t recorded_years() const noexcept { return history.size(); }
    [[nodiscard]] const std::vector<AnnualSnapshot>& get_history() const noexcept { return history; }


};