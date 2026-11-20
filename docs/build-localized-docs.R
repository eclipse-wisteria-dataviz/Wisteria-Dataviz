if (nchar(system.file(package="pacman")) == 0)
{
  install.packages("pacman")
}
library(pacman)
pacman::p_load(tidyverse, this.path, glue, fs)

docFolder <- this.path::this.dir()

source(glue("{docFolder}/wisteria-dataviz-manual/R/qmd2po.R"))

#' @title Build localized documentation for a given language
#'
#' @description
#' Runs both [qmd2po_folder()] and [po2qmd_folder()] to generate a `.po`
#' translation catalog and build a translated user manual for the specified
#' language.
#' Produces output under `locale/docs/<lang>/` and `<docFolder>/<lang>/...`
#'
#' @param docFolder Path to the base documentation folder (e.g. `"docs"`).
#' @param lang Two-letter language code (e.g. `"es"`, `"de"`, `"fr"`).
#' @param exclude_pattern Optional regex pattern of QMD files to skip.
#' @param updatePoFile `TRUE` (the default) to update the PO file from the
#' latest source documents.
#'
#' @examples
#' \dontrun{
#' build_translated_docs("docs", "es")
#' }
#'
#' @export
build_translated_docs <- function(docFolder, lang, exclude_pattern = NULL,
                                  updatePoFile = TRUE)
  {
  # output folder under locale/docs/<lang>
  out_dir <- file.path(dirname(docFolder), "locale", "docs", lang)
  dir.create(out_dir, recursive = TRUE, showWarnings = FALSE)
  dir.create(glue("{docFolder}/{lang}"), recursive = TRUE, showWarnings = FALSE)

  message(glue::glue("🌐 Building documentation for language: {lang}"))
  message(glue::glue("PO output: {out_dir}/{lang}.po\n"))

  # copy auxiliary files
  file_copy(glue("{docFolder}/_variables.yml"),
            glue("{docFolder}/{lang}/_variables.yml"),
            TRUE)

  # --- Step 1: Generate updated PO catalog
  if (updatePoFile)
    {
    qmd2po_folder(
      input_dir = glue::glue("{docFolder}/wisteria-dataviz-manual"),
      output_po = glue::glue("{out_dir}/{lang}.po"),
      lang = lang,
      use_context = TRUE,
      dry_run = FALSE,
      exclude_pattern = exclude_pattern)
    }

  # --- Step 2: Copy source docs and apply translations to them
  po2qmd_folder(
    input_dir = glue::glue("{docFolder}/wisteria-dataviz-manual"),
    output_dir = glue::glue("{docFolder}/{lang}/wisteria-dataviz-manual"),
    po = glue::glue("{out_dir}/{lang}.po"),
    exclude_pattern = exclude_pattern)
  po2qmd_folder(
    input_dir = glue::glue("{docFolder}/release-notes"),
    output_dir = glue::glue("{docFolder}/{lang}/release-notes"),
    po = glue::glue("{out_dir}/{lang}.po"),
    exclude_pattern = exclude_pattern)

  # --- Step 3: Build it
  child_env <- new.env(parent = environment())
  list2env(list(docsLanguage = lang), envir = child_env)
  sys.source(glue::glue("{docFolder}/build-help-projects.R"), envir = child_env)

  message(glue::glue("\n✅ Completed '{lang}' documentation build."))
  invisible(out_dir)
  }

# Build the localized help
##########################

# Spanish
build_translated_docs(docFolder, "es", updatePoFile = TRUE)
