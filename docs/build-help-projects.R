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

docFolder <- this.path::this.dir()
manualFolder <- glue("{docFolder}/wisteria-dataviz-manual")

source(glue("{manualFolder}/R/cover-art.R"))
source(glue("{manualFolder}/R/appdown.r"))

createPolaroidCover(glue("{manualFolder}/images/non-generated/cover-placeholder-1.png"),
                    glue("{manualFolder}/images/non-generated/cover-placeholder-2.png"),
                    glue("{manualFolder}/images/non-generated/cover-placeholder-3.png"),
                    glue("{manualFolder}/images/non-generated/cover.png"),
                    c("[Caption 1]", "[Caption 2]", "[Caption 3]"))

thumbnailWidth <- "256"

# delete previous builds
unlink(glue("{manualFolder}/docs-manual"), recursive=T)

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
  }

clearFolders()

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
