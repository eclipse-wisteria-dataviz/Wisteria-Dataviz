/** @addtogroup Analyses
    @brief Statistical analyses that produce tables of results.
    @date 2005-2026
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef WISTERIA_DESCRIPTIVE_STATISTICS_H
#define WISTERIA_DESCRIPTIVE_STATISTICS_H

#include "analysis.h"

namespace Wisteria::Analyses
    {
    /** @brief Summarizes continuous columns, optionally split by the groups of a
            categorical column.
        @details The results table has one row per column (or per column and group).
            Its columns are:
            - N: the number of valid (finite) values.
            - Missing: the number of missing (NaN) values.
            - Mean.
            - Std. Dev.: the sample standard deviation (N − 1).
            - Min.
            - Q1, Median, Q3: quartiles calculated with Tukey's hinges.
            - Max.

            Statistics that cannot be calculated (e.g., the standard deviation of
            fewer than two values) are NaN.

            A column that isn't a continuous column in the dataset is skipped (with a warning),
            but stays in the options. It is summarized again once the dataset has it.

            References:\n
            Tukey, J. W. (1977). *Exploratory Data Analysis*. Addison-Wesley.\n
            Hoaglin, D. C., Mosteller, F., & Tukey, J. W. (Eds.). (1983).
            *Understanding Robust and Exploratory Data Analysis*. Wiley.*/
    class DescriptiveStatistics final : public Analysis
        {
      public:
        /// @returns The type name used in project files.
        [[nodiscard]]
        static wxString GetTypeNameStatic()
            {
            return L"descriptive-statistics";
            }

        /// @private
        [[nodiscard]]
        wxString GetTypeName() const final
            {
            return GetTypeNameStatic();
            }

        /// @private
        [[nodiscard]]
        wxString GetDisplayTypeName() const final
            {
            return _(L"Descriptive Statistics");
            }

        /// @private
        [[nodiscard]]
        std::unique_ptr<Analysis> Clone() const final
            {
            return std::make_unique<DescriptiveStatistics>(*this);
            }

        /// @private
        [[nodiscard]]
        std::vector<AnalysisResultTable> Run(const Data::Dataset& source,
                                             std::vector<wxString>& warnings) const final;

        /// @private
        void SaveOptions(wxSimpleJSON::Ptr_t& node) const final;

        /// @private
        void LoadOptions(const wxSimpleJSON::Ptr_t& node) final;

        /// @returns The continuous columns to summarize.
        [[nodiscard]]
        const std::vector<wxString>& GetColumns() const noexcept
            {
            return m_columns;
            }

        /// @brief Sets the continuous columns to summarize.
        /// @param columns The column names.
        void SetColumns(std::vector<wxString> columns) { m_columns = std::move(columns); }

        /// @returns The categorical column to group by, or an empty string for no grouping.
        [[nodiscard]]
        const wxString& GetGroupColumn() const noexcept
            {
            return m_groupColumn;
            }

        /// @brief Sets the categorical column to group by.
        /// @param column The column name, or an empty string for no grouping.
        void SetGroupColumn(const wxString& column) { m_groupColumn = column; }

        /// @brief Whether to include the number of valid values.
        bool m_includeN{ true };
        /// @brief Whether to include the number of missing values.
        bool m_includeMissing{ true };
        /// @brief Whether to include the mean.
        bool m_includeMean{ true };
        /// @brief Whether to include the sample standard deviation.
        bool m_includeStdDev{ true };
        /// @brief Whether to include the minimum.
        bool m_includeMin{ true };
        /// @brief Whether to include Q1, the median, and Q3.
        bool m_includeQuartiles{ true };
        /// @brief Whether to include the maximum.
        bool m_includeMax{ true };

        /// @returns @c true if any statistic is included.
        [[nodiscard]]
        bool HasStatistics() const noexcept
            {
            return m_includeN || m_includeMissing || m_includeMean || m_includeStdDev ||
                   m_includeMin || m_includeQuartiles || m_includeMax;
            }

      private:
        std::vector<wxString> m_columns;
        wxString m_groupColumn;
        };
    } // namespace Wisteria::Analyses

/** @}*/

#endif // WISTERIA_DESCRIPTIVE_STATISTICS_H
