BEGINFILE {
  primed = 0

  # Substitutions to "parse" arguments to ```print.

  printsubs["^z:(-?[[:digit:].]+)$"] = "zoom \\1"
  printsubs["^rx:(-?[[:digit:].]+)$"] = "rotate \\1"
  printsubs["^ry:(-?[[:digit:].]+)$"] = "rotate 0 \\1"
}

function print_program()
{
  print "@latex"
  print "\\begin{lstlisting}"

  sub(/([[:space:]]*\n)+$/, "", program)
  print program

  print "\\end{lstlisting}"
  print "@end latex"
}

function substitute_directive(from, to, pre, post)
{
  a = "[" pre "][[:space:]]*" from ":[[:space:]]*"
  b = pre to "{"

  $0 = gensub(a "([^" post "]+)([" post "])", b "\\1}\\2", "g")

  if (sub(a, b)) {
    in_directive = 1
    in_directive_post = post
  }
}

function substitute_weight(from, to, inside)
{
  while(1) {
    if (in_weight) {
      a = gensub("(" inside ")" from "([[:space:].,\"')]|$)", "\\1}\\2", 1)
      if ($0 != a) {
        in_weight = 0
        $0 = a

        continue
      }
    }

    if (!in_weight) {
      a = gensub("(^|[[:space:].,\"'(])" from "(" inside ")", "\\1" to "{\\2", 1)
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
  if (in_figure) {
    if (text ~ /@caption{/) {
      text = text "}"
    }

    text = text "\n@end float\n"
    in_figure = 0
  } else if (in_table) {
    text = text "\n@end table\n"
    in_table = 0
  } else if (in_enumerate) {
    text = text "\n@end enumerate\n"
    in_enumerate = 0
  } else if (in_itemize) {
    text = text "\n@end itemize\n"
    in_itemize = 0
  } else if (in_quotation) {
    text = text "\n@end quotation\n"
    in_quotation = 0
  } else if (in_indentedblock) {
    text = text "\n@end indentedblock\n"
    in_indentedblock = 0
  }
}

$0 ~ "^[[:space:]]*// ---[[:space:]]*" target {
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

  sub(/^[[:space:]]*\/\/[[:space:]]?/, "")

  # Lists and tables

  if (/^ {2,}/ && (in_indent || !(in_example || in_graph))) {
    in_indent = 1

    sub(/^ */, "")

    # Table item

    if (in_figure) {
      if (!(text ~ /@caption\{/)) {
        text = text "@caption{"
      }
    } else if (split($0, v, ":=") == 2) {
      if (!in_table) {
        in_table = 1
        text = text "\n@table @code"
      }

      text = text "\n@item "
      $0 = v[1] "\n" v[2]
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
    } else if (!in_table \
               && !in_enumerate \
               && !in_itemize \
               && !in_quotation \
               && !in_indentedblock) {
        in_indentedblock = 1
        text = text "\n@indentedblock\n"
    }
  } else if (in_indent) {
    in_indent = 0
    close_list()
  }

  if (/^Figure:/) {
    text = text gensub(/^Figure:[[:space:]]*(.*)$/, "@float Figure,\\1\n", 1)
    in_figure = 1
  } else if (/^```print/) {
    a = prefix "." ++figures
    n = split($0, v, ";")

    text = text "@center @image {" a ",145mm}\n"

    in_print = (bindir "/db/gammadb -q --batch " " -c \"window " a "\"")

    if (n > 2) {
      in_print = (in_print                                      \
                  " -c \"resize " ((n - 1) * 500) " 500\""      \
                  " -c \"split horizontally " (n - 1) "\"")

      for (i = 2; i <= n; i++) {
        in_print = (in_print \
                    " -c \"focus " (i - 1) "\""                  \
                    " -c \"target out" (i - 1) "\""              \
                    " -c \"view orthographic\"")

        m = split(v[i], u, ",")

        for (j = 1; j <= m; j++) {
          for (r in printsubs) {
            s = gensub(r, printsubs[r], 1, u[j])

            if (s != u[j]) {
              in_print = in_print " -c \"" s "\""
              break
            }
          }
        }
      }
    }

    in_print = (in_print                                \
                " -c \"set args -x scheme /dev/stdin\"" \
                " -c \"run\""                           \
                " -c \"print " a ".pdf\"")

  } else if (/^```graph/) {
    a = prefix "." ++figures
    text = text "@noindent\n@center @image {" a "}\n"

    in_graph = (/,neato/ ? "neato" : "dot") " -Tpdf -o " a ".pdf"
    print "digraph {" | in_graph

    if (/,lr/) {
      print "rankdir=\"LR\"" | in_graph
    }

    if (/,hier/) {
      print "mode=\"hier\"" | in_graph
    }

    print "node [shape=box, width=0.35, height=0.35]" | in_graph
    print "node [penwidth=0.5, fontname=\"mono\", fontsize=8]" | in_graph
    print "edge [penwidth=0.5, arrowsize=0.5]" | in_graph
    print "edge [fontname=\"sans\", fontsize=8]" | in_graph
  } else if (/^```/) {
    if (in_graph) {
      print "}" | in_graph
      close(in_graph)

      in_graph = ""
      dummy_nodes = 0
    } else if (in_print) {
      close(in_print)
      in_print = ""
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
  } else if (in_print) {
    print $0 | in_print
  } else if (in_example) {
    text = text "\n" $0
  } else  {
    # Structure

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

    substitute_weight("\\*\\*", "@strong", "[[:alnum:]]")
    substitute_weight("\\*", "@emph", "[[:alnum:]]")
    substitute_weight("\\$", "@math", "[[:graph:]]")

    if (in_directive && sub("[" in_directive_post "]", "}" in_directive_post)) {
      in_directive = 0
    }

    if (sub(/[[:space:]]*\^\[/, "@footnote{")) {
      in_footnote = 1
    }

    if (in_footnote && sub(/\]/, "}")) {
      in_footnote = 0
    }

    # Whole sentence cross-references

    substitute_directive("Ref", "  @xref", "", ".")

    # End of sentence or parenthesized cross-references

    substitute_directive("ref", "@pxref", "(", ")")
    substitute_directive("ref", " @pxref", ";", ".")
    substitute_directive("ref", " @pxref", ",", ".")
    substitute_directive("ref", " @ref", "", ".,")

    if (sub(/^[[:space:]]*anchor:[[:space:]]*/, "@anchor{")) {
      sub(/$/, "}")
    }

    # Definition

    substitute_directive("def", " @dfn", "", "[:space:][:punct:]")

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
    close_list()
    flush_text()

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
