if (nchar(system.file(package="pacman")) == 0)
  {
  install.packages("pacman")
  }
library(pacman)
pacman::p_load(glue, readr, stringr, fs, lubridate, tidyverse, kableExtra,
               Hmisc, tinytex, this.path, quarto, magick, pdftools, magrittr)

if (nchar(tinytex::tinytex_root()) == 0)
  {
  tinytex::install_tinytex()
  }
# call tinytex::reinstall_tinytex() if you get a "tlmgr: Remote repository is newer than local..." error message

args <- commandArgs(trailingOnly = TRUE)
# Works for key=value args, current value (if set), then a fallback value
parse_arg <- function(key, fallback)
  {
  # 1. Match CLI arg key=value style
  match <- grep(paste0("^", key, "="), args, value = TRUE)
  if (length(match) > 0) {
    value <- sub("^[^=]*=", "", match)
    return(value)
  }

  # 2. If variable already exists in calling environment:
  caller <- parent.frame()
  if (exists(key, envir = caller, inherits = FALSE)) {
    return(get(key, envir = caller))
  }

  # 3. Otherwise return the fallback default
  return(fallback)
  }

# Command line from RScript to build a translated manual
# (its folder must already be created by build-localized-docs.R)
# Rscript build-help-projects.R --args docsLanguage=es
docsLanguage <- parse_arg("docsLanguage", "")

scriptFolder <- this.path::this.dir()
docFolder <- this.path::this.dir()
# build subfolder of translated docs (if requested)
if (nzchar(docsLanguage) && docsLanguage != "en") {
  docFolder <- file.path(docFolder, docsLanguage)
  message(str_glue("🏗️ Building translated subfolder '{docsLanguage}' → {docFolder}"))
}
manualFolder <- glue("{docFolder}/wisteria-dataviz-manual")

source(glue("{scriptFolder}/wisteria-dataviz-manual/R/cover-art.R"))
source(glue("{scriptFolder}/wisteria-dataviz-manual/R/appdown.r"))

createPolaroidCover(glue("{manualFolder}/images/non-generated/cover-placeholder-1.png"),
                    glue("{manualFolder}/images/non-generated/cover-placeholder-2.png"),
                    glue("{manualFolder}/images/non-generated/cover-placeholder-3.png"),
                    glue("{manualFolder}/images/non-generated/cover.png"),
                    c("[Caption 1]", "[Caption 2]", "[Caption 3]"))

thumbnailWidth <- "256"

# delete previous builds
unlink(glue("{manualFolder}/docs-manual"), recursive=T)
unlink(glue("{docFolder}/release-notes/docs"), recursive=T)

# delete intermediate files from the PDF build
clearFolders <- function()
  {
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.aux"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.log"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.toc"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.tex"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.bcf"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.idx"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.lof"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.lot"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.bbl"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.blg"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.ilg"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.ind"))
  unlink(glue("{manualFolder}/wisteria-dataviz-manual.run.xml"))
  unlink(glue("{manualFolder}/index.ptc"))
  unlink(glue("{manualFolder}/ding.mp3"))

  unlink(glue("{docFolder}/release-notes/fonts"), recursive=T)
  }

clearFolders()

# Release Notes
###############

# Note that this document has its own LaTeX files (i.e., does not copy them from wisteria-dataviz-manual).
# This document doesn't have any front or back matter, so its preamble TeX file doesn't include that.
setwd(glue("{docFolder}/release-notes/"))
file_copy(glue("{docFolder}/_variables.yml"),
          glue("{docFolder}/release-notes/_variables.yml"),
          TRUE)
file_copy(glue("{manualFolder}/_quarto-release-notes.yml"),
          glue("{docFolder}/release-notes/_quarto.yml"),
          TRUE)
file_copy(glue("{manualFolder}/overviews/new-features.qmd"),
          glue("{docFolder}/release-notes/index.qmd"),
          TRUE)
dir_copy(glue("{manualFolder}/fonts"),
         glue("{docFolder}/release-notes/fonts"),
         TRUE)

quarto::quarto_render(output_format="pdf", as_job=F)
# create a thumbnail for the website
thumbNail <- image_convert(image_read_pdf(glue("{docFolder}/release-notes/docs/release-notes.pdf"), 1),
                           format="png") %>%
  image_scale(thumbnailWidth)
image_write(thumbNail, glue("{docFolder}/release-notes/docs/release-notes-thumb.png"))

unlink(glue("{docFolder}/release-notes/_quarto.yml"))
unlink(glue("{docFolder}/release-notes/_variables.yml"))
unlink(glue("{docFolder}/release-notes/index.qmd"))

# User Manual
#############

setwd(manualFolder)
file_copy(glue("{docFolder}/_variables.yml"),
          glue("{manualFolder}/_variables.yml"),
          TRUE)

quarto::quarto_render(output_format="pdf", as_job=F, profile="manual")
thumbNail <- image_convert(image_read_pdf(glue("{manualFolder}/docs-manual/wisteria-dataviz-manual.pdf"), 1),
                           format="png") %>%
  image_scale(thumbnailWidth)
image_write(thumbNail, glue("{manualFolder}/docs-manual/wisteria-dataviz-manual-thumb.png"))
# note that the online build uses the thumbnail that was just generated
quarto::quarto_render(output_format="html", as_job=F, profile="online")

unlink(glue("{manualFolder}/_variables.yml"))

# final cleanup
clearFolders()
