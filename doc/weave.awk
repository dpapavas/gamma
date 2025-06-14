BEGIN {
  highlight = "source-highlight -s C -f texinfo"
}

BEGINFILE {
  primed = 0
}

function print_program()
{
  print ""
  print "@tex"
  print "\\moveleft 5pt \\hbox{"
  print "  \\vbox{"
  print "    \\hrule width 0.5in height 0.4pt"
  print "    \\hbox{\\vrule height 4pt width 0.4pt}"
  print "  }"
  print "}"
  print "@end tex"

  print "@exampleindent 0"

  sub(/([[:space:]]*\n)+$/, "", program)
  printf "%s", program | highlight
  close(highlight)

  print "\n@exampleindent 2"  

  print "@tex"
  print "\\moveright 5pt \\hbox to \\hsize{"
  print "  \\hfill"
  print "  \\vbox{\\hrule width 0.5in height 0.4pt}%"
  print "  \\vrule height 4pt width 0.4pt"
  print "}"
  print "@end tex"
  print ""
}

function substitue_directive(from, to, pre, post)
{
  a = "[" pre "][[:space:]]*" from ":[[:space:]]*"
  b = pre to "{"

  $0 = gensub(a "([^.]*)[" post "]", b "\\1}" post, "g")

  if (sub(a, b)) {
    in_directive = 1
    in_directive_post = post
  }
}

function substitute_weight(from, to)
{
  while(1) {
    if (in_weight) {
      a = gensub("([^[:space:]])" from "([[:space:].,)]|$)", "\\1}\\2", 1)
      if ($0 != a) {
        in_weight = 0
        $0 = a

        continue
      }
    }

    if (!in_weight) {
      a = gensub("(^|[[:space:].,(])" from "([^[:space:]])", "\\1" to "{\\2", 1)
      if ($0 != a) {
        in_weight = 1
        $0 = a

        continue
      }
    }

    break
  }
}

function flush_text()
{
  if (text) {
    sub(/\n*$/, "\n", text)

    print text
    text = ""
  }
}

function close_list()
{
    if (in_table) {
      text = text "@end table\n"
      in_table = 0
    } else if (in_enumerate) {
      text = text "@end enumerate\n"
      in_enumerate = 0
    } else if (in_itemize) {
      text = text "@end itemize\n"
      in_itemize = 0
    } else if (in_quotation) {
      text = text "@end quotation\n"
      in_quotation = 0
    }
}

/^[[:space:]]*\/\/ ---/ {
  primed = !primed
  next
}

!primed { next }

# Commented text

/^[[:space:]]*\/\// {
  if (in_program) {
    if (program) {
      print_program()
      program = ""
    }

    in_program = 0
  }

  if (!in_text) {
    in_text = 1;
  }

  sub(/^[[:space:]]*\/\/[[:space:]]/, "")

  # Lists and tables

  if (/^ {2,}/ && (in_indent || !(in_example || in_graph))) {
    in_indent = 1

    sub(/^ */, "")

    # Table item

    if (split($0, v, ":=") == 2) {
      if (!in_table) {
        in_table = 1
        text = text "\n@table @code"
      }

      text = text "\n@item " v[1] "\n"
      $0 = v[2]
    } else if (match($0, /^[[:digit:]]+\. /)) {
      if (!in_enumerate) {
        in_enumerate = 1
        text = text "\n@enumerate " substr($0, RSTART, RLENGTH - 2)
      }

      text = text "\n@item\n"

      sub(/^[[:digit:]]+\. /, "")
    } else if (/^\* /) {
      if (!in_itemize) {
        in_itemize = 1
        text = text "\n@itemize"
      }

      text = text "\n@item\n"

      sub(/^\* /, "")
    } else if (/^> /) {
      if (!in_quotation) {
        in_quotation = 1
        text = text "\n@quotation\n"
      }

      sub(/^> /, "")
    }
  } else {
    in_indent = 0
    close_list()
  }

  if (/^```graph/) {
    a = prefix "." ++figures
    text = text "@noindent\n@center @image {" a "}"

    in_graph = (/,neato/ ? "neato" : "dot") " -Teps -o " a ".eps"
    print "digraph {" | in_graph

    if (/,lr/) {
      print "rankdir=\"LR\"" | in_graph
    }

    if (/,hier/) {
      print "mode=\"hier\"" | in_graph
    }

    print "node [shape=box, width=0.4, height=0.4]" | in_graph
    print "node [penwidth=0.5, fontname=\"mono\", fontsize=10]" | in_graph
    print "edge [penwidth=0.5, arrowsize=0.5]" | in_graph
    print "edge [fontname=\"sans\", fontsize=11]" | in_graph
  } else if (/^```/) {
    if (in_graph) {
      print "}" | in_graph
      close(in_graph)

      in_graph = ""
      dummy_nodes = 0
    } else {
      if (in_example) {
        text = text "\n" "@end example" "\n"
      } else {
        text = text "@example"
      }

      in_example = !in_example
    }
  } else if (in_graph) {
    if (match($0, /^edge:[[:space:]]/)) {
      $0 = "edge [taillabel=\"" substr($0, RLENGTH + 1) "\"]"
    }

    gsub(/{/, "{rank=same;")

    while(sub(/(^|[[:space:]])\.\.\.([[:space:]]|$)/, "ellipsis" ++dummy_nodes)) {
      print " ellipsis" dummy_nodes "[label=\"...\", style=\"dotted\"] " | in_graph
    }

    while(sub(/(^|[[:space:]])\*([[:space:]]|$)/, "empty" ++dummy_nodes)) {
      print "empty" dummy_nodes "[label=\"\"]" | in_graph
    }

    print $0 | in_graph
  } else if (in_example) {
    text = text "\n" $0
  } else  {
    if (sub(/^# /, "")) {
      sub(/\n*$/, "\n", text)

      text = text "@node " $0 "\n"
      text = text "@chapter "
    } else if (sub(/^## /, "")) {
      sub(/\n*$/, "\n", text)

      text = text "@node " $0 "\n"
      text = text "@section "
    } else if (sub(/^### /, "")) {
      sub(/\n*$/, "\n", text)

      text = text "@node " $0 "\n"
      text = text "@subsection "
    }

    # `code` spans

    while(sub(/`/, (in_code ? "}" : "@code{"))) {
      in_code = !in_code
    }

    # **strong** and *emphasized* text

    substitute_weight("\\*\\*", "@strong")
    substitute_weight("\\*", "@emph")
    substitute_weight("\\$", "@math")

    if (in_directive && sub("[" in_directive_post "]", "}" in_directive_post)) {
      in_directive = 0
    }

    if (sub(/[[:space:]]*\^\[/, "@footnote{")) {
      in_footnote = 1
    } else if (in_footnote && sub(/\]/, "}")) {
      in_footnote = 0
    }

    # Whole sentence cross-references

    substitue_directive("Ref", "  @xref", ".", ".")

    # End of sentence or parenthesized cross-references

    substitue_directive("ref", "@pxref", "(", ")")
    substitue_directive("ref", " @pxref", ";", ".")
    substitue_directive("ref", " @pxref", ",", ".")
    substitue_directive("ref", " @ref", "", ".")

    if (sub(/^[[:space:]]*anchor:[[:space:]]*/, "@anchor{")) {
      sub(/$/, "}")
    }

    # Special glyphs

    gsub(/->/, "@arrow{}")

    # URLs

    $0 = gensub(/"(https?:\/\/[^[:space:]]*)"/, "@url{\\1}", "g")

    # Acronyms

    $0 = gensub(/([[:space:]])?(GNU|CGAL)([[:space:]])?/, "\\1@acronym{\\2}\\3", "g")

    if (text ~ /{$/) {
      text = text $0
    } else {
      text = text " " $0
    }
  }

  next
}

# Blank lines

/^[[:space:]]*$/ {
  if (in_text && text) {
    sub(/\n*$/, "\n", text)

    print text
    text = ""
  }

  if (!in_program) {
    next
  }
}

# Progam source code

{
  if (in_text) {
    in_text = 0
  }

  if (!in_program) {
    close_list()
    flush_text()

    in_program = 1
  }

  program = program $0 "\n"
}

END {
  if (in_program) {
    print_program()
  }
}
