> [!WARNING]
> Texlive 2025 is causing problems with `cleverref`. We recommend you to use an earlier version of texlive while we're working on a fix! To our knowledge, the latest Overleaf version is working fine.

# SaarTex: A Thesis template for Saarland University

**[🍃 > Get Started in Overleaf](https://overleaf.com/docs?snip_uri=https%3A%2F%2Fgitlab.cs.uni-saarland.de%2Ffsr%2Fsaartex%2F-%2Farchive%2Fmaster%2Fsaartex-master.zip) | [> Download](https://gitlab.cs.uni-saarland.de/fsr/saartex/-/archive/master/saartex-master.zip) | [> English Sample](https://gitlab.cs.uni-saarland.de/fsr/saartex/-/wikis/uploads/c47d724a926237cf1b960944646d6fac/saartex_sample_en.pdf) | [> German Sample](https://gitlab.cs.uni-saarland.de/fsr/saartex/-/wikis/uploads/5751bf643ae72924257f1d171d2d4b88/saartex_sample_de.pdf)**

This template aims to make it as easy and hassle-free as possible (not completely hassle-free, you are using LaTeX after all...)
to get started with your thesis.

## Setting up

To get started, make sure that you have an up-to-date distribution of LaTeX installed. To check if that is the case, you can run the following commands:
```
pdflatex -v
lualatex -v
biber -v
```
You should get sensible outputs. 

Now you can go ahead and start writing! Your main file is `Thesis.tex`, but we **strongly recommend spliting up your work into chapters**. You can then include individual chapters using `\include`. This strips down compilation time compared to using `\input`. However, note that `\include` forces a pagebreak and cannot be nested. For a more in-depth explanation about `\include` vs `\input`, see [here](https://www.overleaf.com/learn/latex/Management_in_a_large_project#Inputting_and_including_files).

## How to compile

We support PdfLaTex and LuaLaTex. Your compilation should consist of 4 steps:
```
pdflatex
biber
pdflatex
pdflatex
```
(or, with LuaLaTex, using `lualatex` instead of `pdflatex` respectively). These 4 commands will ensure that the bibliography and any crossreferenced links will be resolved correctly. However, during writeup and if no links/bibliography have changed, you can just compile once for faster compilation. If you're using `latexmk` then you can just go on, it will take over the task of compiling properly multiple times.

## Getting started

1. Pick your language as an option in the very first line in `Thesis.tex`. Available options are `english` and `german`
2. Go to the `Options.tex` file and adjust some of the more common options to your liking.
3. Back in `Thesis.tex`, edit some titlepage elements (`\title`, `\author`, ...)
4. You can use the Sword Declaration that we provide (`Content/Sworn_Declaration.tex` in English or `Content/Eidesstattliche_Erklaerung.tex` in German)
5. We encourage you to write individual chapters in separate files in the `Content/` directory and include them in the main document.
   Our sample chapter has some additional style tips.
6. Literature references can be added to `Literature.bib` in the standard BibLaTex format. Many sites (Google Scholar, Springer Link etc.) offer the option to directly export a citation in this format.

## Good to know

### Alternative Titlepages

We include a good-looking titlepage. However, there are alternatives. You can easily switch to a different style like this:
```latex
% use an alternative style
\TitlePageStyle{TU-HH}

\title{My Title}
\author{Me}
\date{\today}
% ...more options
```
You can find a list of available styles [here](https://www.ctan.org/pkg/uni-titlepage). Note that for some styles, you might need to give more information (such as the subject, the degree etc.) than for others. Alternatively, you can roll your own custom titlepage, but then you are on your own ;)

### TODOs

```latex
\usepackage{todonotes}
% ... later in the document
\todo{Fix this}
```


### Math in sections

Section headings will be used to create a table of contents, not just *inside* the document but also implicitly for your PDF reader. However, those "PDF table contents" do not support advanced mathematical typography. Thus, if you have 
```latex
\section{$\alpha + 1$} 
```
you might get a warning, and if you open the resulting PDF file in a PDF reader, the integrated table of contents might be messed up. The solution is to find a suitable replacement in unicode characters:
```latex
\section{\texorpdfstring{$\alpha + 1$}{α + 1}}
```

### Credits

This template was written by Friedrich Günther
and extended by Alexander Rogovskyy.
