%%-*- LaTeX -*-%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% Ausarbeitung.cls
%
% Klassendatei fuer Projektarbeiten, Bachelorarbeiten, Masterarbeiten, Praxisberichte
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Title:			ausarbeitung.cls
% Author:			Ulrich Klauck, Manuel Blum (HTW Aalen)
% Credits to:		Matthias Müller <matthias.mueller@htw-aalen.de>, Prof. Schiefer
% Modifiied by:		Sebastian Stigler    
% Date:			2008/12/4
% Last Rev.:		2016/12/01
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% Identify the class.
\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{ausarbeitung}[2008/07/02 Ausarbeitung HS Aalen]

\PassOptionsToClass{%
	12pt, % Schriftgröße [10pt, 11pt, 12pt]
	a4paper, % Papierformat [a4paper, a5paper, b5paper, letterpaper, legalpaper]
	onecolumn, % Ein- oder zweispaltig [onecolumn, twocolumn]
	final, % Entwurf oder finale Version [draft, final]
	ngerman, % neue deutsche Rechtschreibung
	reqno, % Nummer bei Formeln links statt rechts [leqno, reqno]
	fleqn, % Formeln linksbündig [fleqn]
	twoside, % ein- oder zweiseitig [oneside, twoside]
	openright, % wo Kapitel beginnen [openright, openany]
	parskip=2ex   % half*,
	headings=big
}{scrbook}

% Default option---pass to scrbook.cls, since that is the base.
\DeclareOption*{%
	\PassOptionsToClass{\CurrentOption}{scrbook}%
}

\ProcessOptions\relax

% Load the base class and required packages.
\LoadClassWithOptions{scrbook}

% ---------------------------------------------------------------------------------

% \RequirePackage{ucs} % incompatible to biblatex
\RequirePackage[utf8]{inputenc}  % use utf8 instead of utf8x 
\RequirePackage[T1]{fontenc}
\RequirePackage[ngerman, english]{babel}
\RequirePackage[babel, german=quotes]{csquotes}

\RequirePackage{lmodern}   % lmodern, times, ...

\RequirePackage{microtype}

% Überprüfen, ob notwendig

\RequirePackage[automark]{scrpage2}
\RequirePackage{calc}
\RequirePackage{array}
\RequirePackage{newcent}
\RequirePackage{setspace}  

\RequirePackage{blindtext}  
  
\RequirePackage{placeins}
\RequirePackage{float}
\RequirePackage[final,colorlinks=true,citecolor=blue]{hyperref}
% --- Ende der zu überprüfenden Liste
% ----------------------------------------------

% Grafik

\RequirePackage{graphicx}
\RequirePackage{pgf}
\RequirePackage{caption}[2008/08/24]
\RequirePackage{subcaption}


% Mathe

\RequirePackage{amsmath}
\RequirePackage{amssymb}
\RequirePackage{amstext}
\RequirePackage{amsthm}
\RequirePackage{mathtools}
\RequirePackage{nicefrac}
\RequirePackage{siunitx}

% Algorithmen und Quellcode

\RequirePackage[formats]{listings}
\RequirePackage{algorithm}
\RequirePackage{algpseudocode}
\RequirePackage{struktex}
\RequirePackage{filecontents}
\RequirePackage{forloop}
\RequirePackage{ifthen}
\RequirePackage{cancel}

% Tabellen

\RequirePackage{longtable}
\RequirePackage{tabularx}
\RequirePackage{multirow}
\RequirePackage{multicol}
\RequirePackage{booktabs}

% Ein paar Symbole

\RequirePackage{gensymb}
\RequirePackage{newunicodechar}
\newunicodechar{°}{\degree}
\RequirePackage{marvosym}

% Indizes
\RequirePackage{makeidx}\makeindex
\RequirePackage[withpage, printonlyused, smaller]{acronym}

% Literaturverzeichnis
\PassOptionsToPackage{toc=bib}{biblatex}
\usepackage[%
            style=numeric,%
            %style=alphabetic,%
            %style=authoryear,%
            %style=authortitle,%
            %style=verbose,%
            %style=draft,%
            %%
            %autocite=plain,%
            %autocite=inline,%
            %autocite=footnote,%
            %autocite=supercript,%
            backend=biber
            ]{biblatex}
% ---------------------------------------------------------------------------------

\lstloadlanguages{R,Java,Python,[ISO]C++,[ANSI]C++,[ANSI]C}

\lstset{
	inputencoding=utf8,
	extendedchars=\true,
%
	numbers=left,
	tabsize=2,
%
	captionpos=b,
	breaklines=true,
	breakatwhitespace=true,
	%prebreak=\raisebox{0ex}[0ex][0ex]{\color{gray}\ensuremath{\rhookswarrow}},
	%postbreak=\raisebox{0ex}[0ex][0ex]{\color{gray}\ensuremath{\rcurvearrowse\space}},
%
	xleftmargin=4ex,
	xrightmargin=0ex,
%
	showtabs=false,
	showspaces=false,
	showstringspaces=false,
%
	basicstyle=\ttfamily\color{black}\scriptsize,
	keywordstyle=\ttfamily\color{blue},
	stringstyle=\ttfamily\color{green},
	commentstyle=\ttfamily\color{orange},
	numberstyle=\ttfamily\color{gray},
	identifierstyle=\ttfamily\color{black},
	%directivestyle=\ttfamily\color{red},
	morekeywords={},
	literate=%
		{Ö}{{\"O}}1 
		{Ä}{{\"A}}1 
		{Ü}{{\"U}}1 
		{ß}{{\ss}}2 
		{ü}{{\"u}}1 
		{ä}{{\"a}}1 
		{ö}{{\"o}}1
}

\lstdefinelanguage{R}{% https://stat.ethz.ch/pipermail/r-help/2006-September/113688.html
	morekeywords=[1]{\<<-},%
	sensitive=false,
	morecomment=[l]{\#},
	morecomment=[s]{/*}{*/},
	morestring=[b]",
}[keywords,comments,strings]

\lstdefinelanguage{CSS}{
	morekeywords={color,display},
	sensitive=false,
	morecomment=[l]{//},
	morecomment=[s]{/*}{*/},
	morestring=[b]",
}

\lstdefinelanguage{C}{%%
%	morekeywords={color,display},
	sensitive=false,
	morecomment=[l]{//},
	morecomment=[s]{/*}{*/},
	morestring=[b]"
}


% Error information should be more than the default.
\setcounter{errorcontextlines}{5}

% ---------------------------------------------------------------------------------
% Pagestyle

\pagestyle{scrheadings} 
\clearscrheadfoot
\ihead{\headmark}
\ohead{\pagemark}
\raggedbottom

%\onehalfspacing % Zeilenabstand: 1,5
\setlength\parskip{2ex}

% Kein Einzug bei neuem Absatz
\setlength{\parindent}{0em}

% ---------------------------------------------------------------------------------
% Titel

\def\@depurl{https://www.hs-aalen.de/faculties/2}
\def\depurl#1{\def\@depurl{#1}}

\def\@depname{Informatik}
\def\depname#1{\def\@depname{#1}}

\newcommand{\departmentaddress}{Informatik}
\newcommand{\depuniversity}{Hochschule Aalen}

%\def\@doctype{}%
\def\doctype#1{\def\@doctype{#1}}%

%\def\@examinerA{}
\def\examinerA#1{\def\@examinerA{#1}}%

%\def\@examinerB{}
\def\examinerB#1{\def\@examinerB{#1}}%

\def\AbgabeDatumTok{\@empty}

%\def\@matrikelnr{}
\def\matrikelnr#1{\def\@matrikelnr{#1}}%

%\def\@companyname{}
\def\companyname#1{\def\@companyname{#1}}%

\renewcommand\maketitle{\begin{titlepage}%
	\sffamily
	\begin{flushright}
		\includegraphics[width=.75\textwidth]{images/htw-aalen.pdf}
	\end{flushright}
	\vskip 2cm
	\begin{flushleft}
		\par \large \color{gray}
		\ifx\@doctype\undefined%
%
		\else%
		\@doctype\\%
		Studiengang\xspace\@depname\xspace\\[0ex]
		\fi%
		\par \rule{\textwidth}{0.25pt}

	\end{flushleft}
%
	\bigskip
	\bigskip
	\bigskip
	\begin{center}
		\ifx\@title\undefined%
		%
		\else
		\par\Huge\textbf{\@title}\bigskip%
		\fi%
		\ifx\@companyname\undefined%
%
		\else%
		\par \large \color{black}%
		bei \par%
		\Large\@companyname\bigskip%
		\fi%	
		\ifx\@author\undefined%
%
		\else%
		\par \large \color{black}%
		von \par%		
		\Large\@author%
		\fi%
		\ifx\@matrikelnr\undefined%
%
        \else%
        \par\Large \color{gray}%
        \@matrikelnr%
        \fi%
  	\end{center}
	\vfill
%
%
	\begin{tabular}{ll}
		\ifx\@examinerA\undefined
		\else
		Betreuender Professor:	& \@examinerA \\
		\fi
		\ifx\@examinerB\undefined
		\else
		Zweitprüfer:			& \@examinerB \\
		\fi
		\ifx\@date\undefined
		\else
		& \\
		Einreichungsdatum:		& \@date \\
		\fi
	\end{tabular}
\end{titlepage}
}

% ---------------------------------------------------------------------------------
% Praxisstelle / Eidesstattliche Versicherung


%\def\@industrialsector{}
\def\industrialsector#1{\def\@industrialsector{#1}}%

%\def\@department{}
\def\department#1{\def\@department{#1}}%

%\def\@companystreet{}
\def\companystreet#1{\def\@companystreet{#1}}%

%\def\@companycity{}
\def\companycity#1{\def\@companycity{#1}}%

%\def\@advisorname{}
\def\advisorname#1{\def\@advisorname{#1}}%

%\def\@advisorphone{}
\def\advisorphone#1{\def\@advisorphone{#1}}%

%\def\@advisoremail{}
\def\advisoremail#1{\def\@advisoremail{#1}}%

\def\makeworkplace{%

\chapter*{Angaben zur Firma}
\addcontentsline{toc}{chapter}{Angaben zur Firma}
	\begin{tabular}{ll}
		\ifx\@companyname\undefined
		\else
		\textbf{Unternehmen:}		& \@companyname \\
		\fi
		\ifx\@industrialsector\undefined
		\else
		\textbf{Branche:}			& \@industrialsector \\
		\fi
		\ifx\@department\undefined
		\else
		\textbf{Abteilung:}			& \@department \\
		\fi
		\ifx\@companystreet\undefined
		\else
		\textbf{Adresse:}			& \@companystreet \\
		\fi
		\ifx\@companycity\undefined
		\else
									& \@companycity \\
		\fi
		 & \\
		\ifx\@advisorname\undefined
		\else
		\textbf{Betreuer:}			& \@advisorname \\
		\fi
		\ifx\@advisorphone\undefined
		\else
		\textbf{Telefon:}			& \@advisorphone \\
		\fi
		\ifx\@advisoremail\undefined
		\else
		\textbf{E-Mail:}			& \@advisoremail \\
		\fi
	\end{tabular}%
}

\def\makeaffirmation{%
\chapter*{Eidesstattliche Erkl\"arung}
\addcontentsline{toc}{chapter}{Eidesstattliche Erkl\"arung}

Hiermit erkl\"are ich, \textbf{\@author}, dass ich die vorliegenden Angaben in dieser Arbeit 
\ifx\@companyname\undefined
\else
bei \textbf{\@companyname}
\fi
wahrheitsgetreu und selbständig verfasst habe.

Weiterhin versichere ich, keine anderen als die angegebenen Quellen und
Hilfsmittel benutzt zu haben, dass alle Ausführungen, die anderen 
Schriften w\"ortlich oder sinngem\"a\ss{} entnommen wurden, kenntlich 
gemacht sind und dass die Arbeit in gleicher oder \"ahnlicher Fassung 
noch nicht Bestandteil einer Studien- oder Prüfungsleistung war.

\vspace{4em}
\makebox[\textwidth/2]{Ort, Datum} Unterschrift (Student)

\ifx\@companyname\undefined
\else
\ifx\@advisorname\undefined
\else
\section*{Best\"atigung der inhaltlichen Richtigkeit}
Der vorliegende Bericht wurde durch den zust\"andigen Betreuer 
\textbf{\@advisorname} korrekturgelesen und auf inhaltliche Korrektheit 
gepr\"uft.

\vspace{4em}
\makebox[\textwidth/2]{Ort, Datum} Unterschrift (Betreuer)
\fi
\fi
}

% --------------------------------------------------------------------------------------------------
%
% Übernahmen aus AKAD

\RequirePackage{marginnote}%

\newif\if@freescripts
\if@freescripts
  \def\icon#1{{\sffamily\bfseries\color{red}#1}}%
  \def\texticon#1{{\sffamily\bfseries\color{red}#1}}%
\fi%@freescripts

\newlength{\Kenumsep}%
\setlength{\Kenumsep}{\z@}

\def\AKADsectionname#1{\@bsphack\gdef\AKAD@sectionname{#1}\@esphack}%
\AKADsectionname{\chaptername~\thesection}%

\def\Kmarkeprefix#1{\@bsphack\def\K@prf{#1}\@esphack}%
\Kmarkeprefix{K}%
\newcounter{KFnum}%
\newcommand{\KF@marke}[2]{%
  {\fontsize{8\p@}{9\p@}\selectfont\textsf{#1}}~%
  {\fontsize{9\p@}{9\p@}\selectfont\texticon{[#2]}}%
}%
\newcommand{\KFmarke}[3][0pt]{%
  \marginnote{\KF@marke{#2}{#3}}[#1]%
  \ignorespaces
}%
\def\mk@K@lbl#1{\xdef\K@lbl{akad.}}%
\newcommand{\K@labref}[2]{%
\mk@K@lbl{#2}%
\csname #1\endcsname\K@lbl%
}%
\newcommand{\Klabel}[2][\K@prf]{\K@labref{label}{#1#2}}%
\newcommand{\Kref}[2][\K@prf]{\K@labref{ref}{#1#2}}%
\newcommand{\Kpageref}[2][\K@prf]{\K@labref{pageref}{#1#2}}%
\newlength{\KFKenumsep}%
\AtBeginDocument{\KFKenumsep\Kenumsep}

\newenvironment{Kontrollfrage}[2][\z@]{%
  \def\enumerate{\Kenum[\KFKenumsep]}%
  \let\endenumerate\endKenum
  \par\smallskip\noindent
  \refstepcounter{KFnum}%
  \mk@K@lbl{\K@prf#2}%
  \label{\K@lbl}%
  \null\KFmarke[#1]{\K@prf}{\arabic{KFnum}}%
    }%
 {\par\smallskip\ignorespacesafterend}%

\newenvironment{Kontrollfrage*}[2][\z@]{%
  \def\enumerate{\Kenum[\KFKenumsep]}%
  \let\endenumerate\endKenum
  \par\smallskip\noindent
  \mk@K@lbl{\K@prf#2}%
  \label{\K@lbl}%
    \null\KFmarke[#1]{\K@prf}{#2}%
    }%
 {\par\smallskip\ignorespacesafterend}%
\newcommand{\KAmarke}[1]{%
  \rlap{\color{KA@rulecolor}\rule[5mm]{8mm}{1.5\p@}}%
  {\normalfont{\Kantwortnumfont#1\ref{\K@lbl}\_\_}Seite~\pageref{\K@lbl}}%
}%
\def\solutionsname{Antworten zu den Kontrollfragen}%
\def\solutionsshortname{Antworten zu\newline den Kontrollfragen}%
\newcommand{\Antwortteil}[1][\solutionsname]{%
  \initAKADsects
  \sectionnewpage%
  \addtocontents{toc}{\protect\setcounter{tocdepth}{3}}%
  \section*{#1}%
  \AKADsectionname{\solutionsshortname}%
  \addtoc@appendix{#1}%
  \def\figurename{\ignorespaces}\def\thefigure{}%
  \def\tablename{\ignorespaces}\def\thetable{}%
  \def\floatnameend{\hspace*{-.25em}}%
  \ignorespaces
}%
\newlength{\KAKenumsep}%
\AtBeginDocument{\KAKenumsep\Kenumsep}
\newif\ifKAmarkebigskipbelow
\KAmarkebigskipbelowfalse
\newenvironment{Kontrollantwort}[2][\K@prf]{%
  \def\enumerate{\Kenum[\KAKenumsep]}%
  \let\endenumerate\endKenum
  \par
  \medskip
  \noindent
  \mk@K@lbl{#1#2}%
  \null\KAmarke{#1}%
  \ifKAmarkebigskipbelow
    \par\noindent
  \else
    \\*
  \fi
  \ignorespaces
  }%
 {\par%
  \ignorespacesafterend}%

\def\introname{Einleitung und Lernziele}%
\def\introshortname{Einleitung/Lernziele}%
\newcommand{\Einleitung}[1][\introname]{%
  \section*{#1}%
  \AKADsectionname{\introshortname}%
  \addcontentsline{toc}{subsection}{#1}%
  \ignorespaces
}%


