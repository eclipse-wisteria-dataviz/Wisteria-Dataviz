Adding a New Analysis
=============================

This is an overview of the files involved and the changes needed when adding a new type of statistical analysis.
The `DescriptiveStatistics` analysis is used as an example throughout.

An analysis runs on one dataset and produces one or more tables of results.
Only its options are saved to the project.
Its results are recalculated whenever the project is loaded or refreshed.
The result tables are never added to the project's datasets (`ReportBuilder::m_datasets`), so they can't be used by graphs.
They are shown in the Analysis section of the sidebar and can be exported as delimited text.

Analysis Class
=============================

Create `src/analyses/descriptivestatistics.h` and `src/analyses/descriptivestatistics.cpp`.
*CMake* picks up new `.cpp` files under `src/` automatically (see `cmake/ListFiles.cmake`).

Derive from `Analyses::Analysis` (`src/analyses/analysis.h`) and implement the following:

- `GetTypeName()`: the `"type"` string used in project files (e.g., `"descriptive-statistics"`).
  Also add a static `GetTypeNameStatic()` that returns the same string, for the factory.
- `GetDisplayTypeName()`: the translated name of the analysis, which is also a good name for a single results table.
- `Clone()`: return `std::make_unique<YourAnalysis>(*this)`.
  The base class's copy constructor is protected, so the derived class's implicit copy constructor works.
  A clone keeps the GUID, so an edited copy replaces the original.
- `Run(const Data::Dataset&, std::vector<wxString>& warnings)`: validate the options against the dataset and build the results.
  - Skip what you can work around (e.g., a column that is missing or no longer continuous), and add a translated message to `warnings`.
    Keep the skipped item in the options, so that it comes back if the dataset has it again.
    The warnings are shown when the analysis is created, edited, or loaded.
  - Throw `std::runtime_error` (with a translated, UTF-8 encoded message) only if the analysis can't be run at all (e.g., none of its columns are found).
  - Return a `std::vector<AnalysisResultTable>`, which is a name plus a `Data::Dataset` for each table.
    Return several tables if the analysis needs them (e.g., loadings, eigenvalues, and communalities for factor analysis).
    The results window shows tabs only when there is more than one table.
  - Use NaN for a statistic that can't be calculated. The grid shows it as an empty cell.
- `SaveOptions()` / `LoadOptions()`: write and read only the type-specific options.
  The common properties (`type`, `name`, `guid`, and `dataset`) are handled by the caller.

Put the calculations in `src/math/statistics.h` where possible, and cite the methods in the doc comments.

Register the type in the factory, `Analysis::Create()` in `src/analyses/analysis.cpp`:

```cpp
if (typeName.CmpNoCase(DescriptiveStatistics::GetTypeNameStatic()) == 0)
    {
    return std::make_unique<DescriptiveStatistics>();
    }
```

JSON Report Support
=============================

`ReportBuilder` doesn't need changes for a new analysis type.
`ReportBuilder::LoadAnalyses()` reads the top-level `"analyses"` array after datasets and constants are loaded.
For each entry, it calls `Analysis::Create(type)`, then `LoadOptions()`, then `RunAnalysis()`.

- An unknown type is dropped, and a pending error message is queued.
- An analysis that fails to run (e.g., its dataset is missing) is kept without results, so its options aren't lost on the next save.

The project's analyses are available from `ReportBuilder::GetAnalyses()`, `AddAnalysis()`, `FindAnalysis(guid)`, `RemoveAnalysis(guid)`, and `RunAnalysis(index)`.

Saving
=============================

`WisteriaDoc::SaveProject()` (`src/app/wisteriadoc.cpp`) writes the common properties of each analysis and calls its `SaveOptions()`.
No changes are needed for a new analysis type.

Dialog
=============================

Create `src/ui/dialogs/editors/yourdlg.h/.cpp` (see `DescriptiveStatsDlg`):

- Derive from `DialogWithHelp`.
- Take a `const ReportBuilder*` (for the dataset list), the analysis to edit, the parent, and a caption.
- Return an edited copy of the analysis from `GetAnalysis()`.
- Connect the controls with validators (`wxGenericValidator`).
- Validate in `TransferDataFromWindow()` (call the base version first), so that the DDX values are current.
- Use `VariableSelectDlg` for column selection.

Application
=============================

In `src/app/wisteriaapp.h`, add a command ID for the analysis.

In `src/app/wisteriaapp.cpp`:

- Add the ID to `GetCommandIdNames()`.
- Add a button to the **Analysis** ribbon tab. Put it in an existing panel (e.g., **Basic Statistics**) or a new one (e.g., **Multivariate**).
  The **Results** panel (Edit and Delete) and the File › Export page's **Export Results** work with any type of analysis.

In `src/app/wisteriaview.h` and `src/app/wisteriaview_analyses.cpp`:

- Declare and implement an `OnYourAnalysis()` handler, following `OnDescriptiveStatistics()`:
  1. Show the dialog.
  2. Make the name unique with `GetUniqueAnalysisName()`.
  3. Call `m_reportBuilder.AddAnalysis()`, then `RunAnalysis()` (showing any error).
  4. Call `AddAnalysisToSection()`.
- Add a `dynamic_cast` branch to `OnEditAnalysis()` that opens the new dialog.

In `src/app/wisteriaview.cpp`, bind the handler for both `wxEVT_RIBBONBUTTONBAR_CLICKED` and `wxEVT_MENU` (next to the Descriptive Statistics bindings).

Add the command to the **Analysis** menu in `res/menus/project-menubar.xrc` and to `res/tellme.xml`.

Documentation
=============================

- Manual: add a section to `docs/wisteria-dataviz-manual/dialogs/analyses.qmd`, with citations for the methods.
- Syntax manual: add the type and its options under `"analyses"` in `docs/syntax-manual/project.qmd`.

Tests
=============================

Add a GUI test file (e.g., `tests/gui-tests/descriptivestatisticstests.cpp`) that covers:

- the results for known values (with the source of the expected values, such as R, noted in a comment)
- edge cases (e.g., missing values, too few observations)
- missing columns throwing `std::runtime_error`
- `Analysis::Create()` for the new type
- a `SaveOptions()` → `LoadOptions()` round trip
- loading the analysis from a project's `"analyses"` array with `ReportBuilder`.
