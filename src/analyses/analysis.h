/** @addtogroup Analyses
    @brief Statistical analyses that produce tables of results.
    @date 2005-2026
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef WISTERIA_ANALYSIS_H
#define WISTERIA_ANALYSIS_H

#include "../data/dataset.h"
#include "../util/guid.h"
#include "../wxSimpleJSON/src/wxSimpleJSON.h"
#include <memory>
#include <vector>

/// @brief Statistical analyses that produce tables of results.
namespace Wisteria::Analyses
    {
    /// @brief A named table of results from an analysis.
    struct AnalysisResultTable
        {
        /// @brief The display name of the table.
        wxString m_name;
        /// @brief The table's data.
        std::shared_ptr<Data::Dataset> m_data;
        };

    /** @brief Base class for an analysis that runs on a dataset and produces
            one or more tables of results.
        @details Only an analysis's options are saved. Its results are recalculated
            from its source dataset when a project is loaded.*/
    class Analysis
        {
      public:
        /// @private
        Analysis() = default;
        /// @private
        Analysis& operator=(const Analysis&) = delete;
        /// @private
        virtual ~Analysis() = default;

        /** @brief Creates an analysis from its type name.
            @param typeName The analysis's type name (e.g., @c "descriptive-statistics").
            @returns The analysis, or @c nullptr if @p typeName is not a known type.*/
        [[nodiscard]]
        static std::unique_ptr<Analysis> Create(const wxString& typeName);

        /// @returns The type name used in project files (e.g., @c "descriptive-statistics").
        [[nodiscard]]
        virtual wxString GetTypeName() const = 0;

        /// @returns The translated name of the analysis type, for display.
        [[nodiscard]]
        virtual wxString GetDisplayTypeName() const = 0;

        /// @returns A copy of the analysis, including its options and results.
        [[nodiscard]]
        virtual std::unique_ptr<Analysis> Clone() const = 0;

        /** @brief Runs the analysis on a dataset.
            @param source The dataset to analyze.
            @param[out] warnings Problems that the analysis worked around
                (e.g., a column that was skipped because it is missing).
            @returns The tables of results.
            @throws std::runtime_error If the analysis can't be run at all
                (e.g., none of its columns are in the dataset).\n
                The exception's @c what() message is UTF-8 encoded.*/
        [[nodiscard]]
        virtual std::vector<AnalysisResultTable> Run(const Data::Dataset& source,
                                                     std::vector<wxString>& warnings) const = 0;

        /** @brief Writes the type-specific options to a JSON node.
            @param node The node to write to.*/
        virtual void SaveOptions(wxSimpleJSON::Ptr_t& node) const = 0;

        /** @brief Reads the type-specific options from a JSON node.
            @param node The node to read from.*/
        virtual void LoadOptions(const wxSimpleJSON::Ptr_t& node) = 0;

        /** @brief Runs the analysis on a dataset and stores its results.
            @param source The dataset to analyze.
            @throws std::runtime_error If the analysis fails.
                The previous results are cleared either way.*/
        void Calculate(const Data::Dataset& source)
            {
            ClearResults();
            m_results = Run(source, m_warnings);
            }

        /// @brief Removes the results and warnings.
        void ClearResults() noexcept
            {
            m_results.clear();
            m_warnings.clear();
            }

        /// @returns The tables from the last call to Calculate().
        [[nodiscard]]
        const std::vector<AnalysisResultTable>& GetResults() const noexcept
            {
            return m_results;
            }

        /// @returns The problems that the last call to Calculate() worked around.
        [[nodiscard]]
        const std::vector<wxString>& GetWarnings() const noexcept
            {
            return m_warnings;
            }

        /// @returns The name of the analysis.
        [[nodiscard]]
        const wxString& GetName() const noexcept
            {
            return m_name;
            }

        /// @brief Sets the name of the analysis.
        /// @param name The name.
        void SetName(const wxString& name) { m_name = name; }

        /// @returns The unique ID of the analysis.
        [[nodiscard]]
        const wxString& GetGuid() const noexcept
            {
            return m_guid;
            }

        /// @brief Sets the unique ID of the analysis.
        /// @param guid The ID.
        void SetGuid(const wxString& guid) { m_guid = guid; }

        /// @returns The name of the dataset that the analysis runs on.
        [[nodiscard]]
        const wxString& GetSourceDatasetName() const noexcept
            {
            return m_sourceDatasetName;
            }

        /// @brief Sets the name of the dataset that the analysis runs on.
        /// @param name The dataset name.
        void SetSourceDatasetName(const wxString& name) { m_sourceDatasetName = name; }

      protected:
        /// @private
        Analysis(const Analysis&) = default;

      private:
        wxString m_name;
        wxString m_guid{ GenerateGuid() };
        wxString m_sourceDatasetName;
        std::vector<AnalysisResultTable> m_results;
        std::vector<wxString> m_warnings;
        };
    } // namespace Wisteria::Analyses

/** @}*/

#endif // WISTERIA_ANALYSIS_H
