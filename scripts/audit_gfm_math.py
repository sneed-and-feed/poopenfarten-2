#!/usr/bin/env python3
"""
GitHub-Flavored Markdown (GFM) & KaTeX Math Compliance Auditor
Enforces strict KaTeX and CommonMark delimiter immunity rules to prevent
"Unable to render expression" errors and CommonMark emphasis hijacking.
"""

import os
import sys
import re

def check_cross_span_collisions(line):
    """
    Detects CommonMark §6.2 underscore emphasis flanking delimiter pairs
    that span across math '$' delimiters on the same line.
    """
    tokens = []
    for i, c in enumerate(line):
        if c == '_':
            prev_c = line[i-1] if i > 0 else ' '
            next_c = line[i+1] if i < len(line)-1 else ' '

            def is_word(ch):
                return ch.isalnum() or ch == '_'
            def is_punct(ch):
                return not ch.isalnum() and not ch.isspace()

            prev_word = is_word(prev_c)
            next_word = is_word(next_c)
            prev_punct = is_punct(prev_c)
            next_punct = is_punct(next_c)
            prev_space = prev_c.isspace()
            next_space = next_c.isspace()

            left_flanking = (not next_space) and ((not next_punct) or (next_punct and (prev_space or prev_punct)))
            right_flanking = (not prev_space) and ((not prev_punct) or (prev_punct and (next_space or next_punct)))

            can_open = left_flanking and (not right_flanking or prev_punct)
            can_close = right_flanking and (not left_flanking or next_punct)

            tokens.append((i, prev_c, next_c, can_open, can_close))

    openers = [t for t in tokens if t[3]]
    closers = [t for t in tokens if t[4]]

    collisions = []
    if openers and closers:
        dollar_pos = [m.start() for m in re.finditer(r'(?<!\\)\$', line)]
        for o in openers:
            for c in closers:
                if o[0] < c[0]:
                    between_dollars = [d for d in dollar_pos if o[0] < d < c[0]]
                    if between_dollars:
                        collisions.append((o, c, between_dollars))
                        break
    return collisions

def run_strict_linter(fpath):
    """
    Runs the strict 19-rule audit against a Markdown file.
    Returns a list of violation dictionaries: {'file', 'line', 'rule', 'detail'}.
    """
    if not os.path.isfile(fpath):
        return [{'file': fpath, 'line': 0, 'rule': 'File Not Found', 'detail': f"File does not exist: {fpath}"}]

    with open(fpath, 'r', encoding='utf-8') as f:
        text = f.read()

    lines = text.splitlines(keepends=True)
    violations = []

    in_code_block = False
    in_display_math_code = False
    in_display_math_dollars = False

    for idx, raw_line in enumerate(lines):
        line_num = idx + 1
        line = raw_line.rstrip('\r\n')
        stripped = line.strip()

        # 1. Display math via ```math block
        if stripped == '```math':
            if not in_display_math_code and not in_code_block:
                in_display_math_code = True
                # Rule 8: Column 0 check
                if not line.startswith('```math'):
                    violations.append({
                        'file': fpath,
                        'line': line_num,
                        'rule': 'Rule 8: Display Math Block Alignment',
                        'detail': f"Opening '```math' is indented (not at Column 0): '{line}'"
                    })
                # Blank line before
                if idx > 0:
                    prev_line = lines[idx-1].rstrip('\r\n').strip()
                    if prev_line != '' and not prev_line.startswith('>'):
                        violations.append({
                            'file': fpath,
                            'line': line_num,
                            'rule': 'Rule 8: Display Math Blank Line Before',
                            'detail': f"No blank line before '```math'. Previous line: '{prev_line}'"
                        })
                continue

        if in_display_math_code:
            if stripped == '```':
                in_display_math_code = False
                # Rule 8: Closing ``` at column 0
                if not line.startswith('```'):
                    violations.append({
                        'file': fpath,
                        'line': line_num,
                        'rule': 'Rule 8: Display Math Block Alignment',
                        'detail': f"Closing '```' is indented (not at Column 0): '{line}'"
                    })
                # Blank line after
                if idx + 1 < len(lines):
                    next_line = lines[idx+1].rstrip('\r\n').strip()
                    if next_line != '' and not next_line.startswith('>'):
                        violations.append({
                            'file': fpath,
                            'line': line_num,
                            'rule': 'Rule 8: Display Math Blank Line After',
                            'detail': f"No blank line after '```'. Next line: '{next_line}'"
                        })
                continue

            # Inside ```math block checks
            if r'\operatorname' in line:
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 11: Disallowed KaTeX Macro \\operatorname',
                    'detail': f"Found '\\operatorname' in display math (disallowed on GitHub, use '\\mathrm'): '{line.strip()}'"
                })
            if re.search(r'\\text(?:rm|bf|normal|it)?\{--\}', line):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 4: KaTeX Ligatures',
                    'detail': f"Found '\\text{{--}}' in display math: '{line.strip()}'"
                })
            mb_matches = re.findall(r'\\mathbf\{([^{}]*)\}', line)
            for inner in mb_matches:
                if re.search(r'=|\+|-|\\pm|\\times|\\le|\\ge|\\approx|\\neq|\\sim|\\to|\\equiv', inner):
                    violations.append({
                        'file': fpath,
                        'line': line_num,
                        'rule': 'Rule 5: KaTeX \\mathbf with Operators',
                        'detail': f"Operator inside \\mathbf: '\\mathbf{{{inner}}}'"
                    })
            if re.search(r'(?<!\\)\*', line):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 7: Math Asterisk Escapes',
                    'detail': f"Raw '*' in display math: '{line.strip()}'"
                })
            if re.search(r'\\left\\\{|\\right\\\}', line):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 15: Unescaped Scalable Curly Delimiters',
                    'detail': f"Found '\\left\\{{' or '\\right\\}}' in display math (use '\\left\\lbrace' / '\\right\\rbrace'): '{line.strip()}'"
                })
            continue

        # Standard fenced code blocks (not math)
        if stripped.startswith('```'):
            in_code_block = not in_code_block
            continue

        if in_code_block:
            continue

        # 2. Display math via $$
        if stripped.startswith('$$'):
            is_blockquote = stripped.startswith('>') or line.lstrip().startswith('>')
            if not in_display_math_dollars:
                in_display_math_dollars = True
                if not is_blockquote and not line.startswith('$$'):
                    violations.append({
                        'file': fpath,
                        'line': line_num,
                        'rule': 'Rule 8: Display Math Block Alignment',
                        'detail': f"Opening '$$' is indented (not at Column 0): '{line}'"
                    })
                if idx > 0:
                    prev_line = lines[idx-1].rstrip('\r\n').strip()
                    if prev_line != '' and not prev_line.startswith('>'):
                        violations.append({
                            'file': fpath,
                            'line': line_num,
                            'rule': 'Rule 8: Display Math Blank Line Before',
                            'detail': f"No blank line before '$$'. Previous line: '{prev_line}'"
                        })
            else:
                in_display_math_dollars = False
                if not is_blockquote and not line.startswith('$$'):
                    violations.append({
                        'file': fpath,
                        'line': line_num,
                        'rule': 'Rule 8: Display Math Block Alignment',
                        'detail': f"Closing '$$' is indented (not at Column 0): '{line}'"
                    })
                if idx + 1 < len(lines):
                    next_line = lines[idx+1].rstrip('\r\n').strip()
                    if next_line != '' and not next_line.startswith('>'):
                        violations.append({
                            'file': fpath,
                            'line': line_num,
                            'rule': 'Rule 8: Display Math Blank Line After',
                            'detail': f"No blank line after '$$'. Next line: '{next_line}'"
                        })
            continue

        if in_display_math_dollars:
            if r'\operatorname' in line:
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 11: Disallowed KaTeX Macro \\operatorname',
                    'detail': f"Found '\\operatorname' in display math (disallowed on GitHub, use '\\mathrm'): '{line.strip()}'"
                })
            if re.search(r'\\text(?:rm|bf|normal|it)?\{--\}', line):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 4: KaTeX Ligatures',
                    'detail': f"Found '\\text{{--}}' in display math: '{line.strip()}'"
                })
            mb_matches = re.findall(r'\\mathbf\{([^{}]*)\}', line)
            for inner in mb_matches:
                if re.search(r'=|\+|-|\\pm|\\times|\\le|\\ge|\\approx|\\neq|\\sim|\\to|\\equiv', inner):
                    violations.append({
                        'file': fpath,
                        'line': line_num,
                        'rule': 'Rule 5: KaTeX \\mathbf with Operators',
                        'detail': f"Operator inside \\mathbf: '\\mathbf{{{inner}}}'"
                    })
            if re.search(r'(?<!\\)\*', line):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 7: Math Asterisk Escapes',
                    'detail': f"Raw '*' in display math: '{line.strip()}'"
                })
            if re.search(r'\\left\\\{|\\right\\\}', line):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 15: Unescaped Scalable Curly Delimiters',
                    'detail': f"Found '\\left\\{{' or '\\right\\}}' in display math (use '\\left\\lbrace' / '\\right\\rbrace'): '{line.strip()}'"
                })
            continue

        # In standard prose / table line
        # Rule 12: 4+ space indented prose (unintended code block in CommonMark)
        if re.match(r'^[ ]{4,}[^ \t\-*+\d>]', line):
            violations.append({
                'file': fpath,
                'line': line_num,
                'rule': 'Rule 12: 4+ Space Indented Prose',
                'detail': f"Prose line indented by 4+ spaces (parsed as indented code block by CommonMark): '{line}'"
            })

        # Rule 1: Backtick math
        if re.search(r'\$`|`\$|\$\s*`|`\s*\$', line):
            violations.append({
                'file': fpath,
                'line': line_num,
                'rule': 'Rule 1: Backtick Math',
                'detail': f"Backtick adjacent to math delimiter in: '{line.strip()}'"
            })

        # Rule 9: Hyphen math
        hm_matches = re.finditer(r'\b([A-Za-z]+)-\$([^\$\n]+)\$', line)
        for hm in hm_matches:
            violations.append({
                'file': fpath,
                'line': line_num,
                'rule': 'Rule 9: Hyphen Math',
                'detail': f"Hyphen outside math: '{hm.group(0)}' in '{line.strip()}'"
            })

        # Rule 10: Leading math in list item prefix
        if re.match(r'^\s*(\d+\.|[-*+])\s+\$', line):
            violations.append({
                'file': fpath,
                'line': line_num,
                'rule': 'Rule 10: Leading Math in List Prefix',
                'detail': f"List item starts with inline math before label: '{line.strip()}'"
            })

        # Rule 17: Inline $math$ inside markdown link anchor text [...](...)
        link_matches = re.finditer(r'\[([^\]]*\$(?:(?!\$).)+?\$[^\]]*)\]\((.*?)\)', line)
        for lm in link_matches:
            violations.append({
                'file': fpath,
                'line': line_num,
                'rule': 'Rule 17: Math Inside Markdown Link Anchor',
                'detail': f"Markdown link contains unrendered '$math$' inside anchor text (GitHub ignores math inside links): '{lm.group(0)}'"
            })

        # Rule 18: Prime-preceded operator subscripts (\bigotimes'_p, \prod'_p)
        if re.search(r'\\(?:bigotimes|prod|coprod|bigoplus)\'_', line):
            violations.append({
                'file': fpath,
                'line': line_num,
                'rule': 'Rule 18: Prime-Preceded Operator Subscript',
                'detail': f"Operator prime immediately preceding subscript (causes CommonMark emphasis open-tag hijack, use '{{\\bigotimes_p}}\''): '{line.strip()}'"
            })

        # Rule 3: Delimiter balance on line (strip inline code first)
        line_no_code = re.sub(r'`[^`]+`', '', line)
        dollars = re.findall(r'(?<!\\)\$', line_no_code)
        if len(dollars) % 2 != 0:
            violations.append({
                'file': fpath,
                'line': line_num,
                'rule': 'Rule 3: Delimiter Balance',
                'detail': f"Unclosed '$' delimiter (found {len(dollars)} '$'): '{line.strip()}'"
            })

        # Rule 19: Cross-span delimiter collisions (CommonMark emphasis across math spans)
        cross_collisions = check_cross_span_collisions(line)
        for col in cross_collisions:
            o, c, between = col
            violations.append({
                'file': fpath,
                'line': line_num,
                'rule': 'Rule 19: Cross-Span Delimiter Collision',
                'detail': f"CommonMark flanking underscore opener at pos {o[0]} ({o[1]}_{o[2]}) pairs with closer at pos {c[0]} ({c[1]}_{c[2]}) crossing math delimiter(s) at {between}"
            })

        # Tokenize inline math
        inline_spans = list(re.finditer(r'(?<!\\)\$(?!\$)(.*?)(?<!\\)\$', line_no_code))
        is_table = line.strip().startswith('|') and line.strip().endswith('|')

        for m in inline_spans:
            math_content = m.group(1)
            full_match = m.group(0)

            # Rule 11: Disallowed \operatorname macro
            if r'\operatorname' in math_content:
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 11: Disallowed KaTeX Macro \\operatorname',
                    'detail': f"Found '\\operatorname' in inline math (disallowed on GitHub, use '\\mathrm'): '{full_match}'"
                })

            # Rule 2: Delimiter whitespace
            if math_content.startswith(' ') or math_content.startswith('\t') or math_content.endswith(' ') or math_content.endswith('\t'):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 2: Delimiter Whitespace',
                    'detail': f"Leading/trailing space in inline math: '{full_match}'"
                })

            # Rule 4: KaTeX ligatures
            if re.search(r'\\text(?:rm|bf|normal|it)?\{--\}', math_content):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 4: KaTeX Ligatures',
                    'detail': f"Found '\\text{{--}}' in inline math: '{full_match}'"
                })

            # Rule 5: \mathbf operators
            mb_matches = re.findall(r'\\mathbf\{([^{}]*)\}', math_content)
            for inner in mb_matches:
                if re.search(r'=|\+|-|\\pm|\\times|\\le|\\ge|\\approx|\\neq|\\sim|\\to|\\equiv', inner):
                    violations.append({
                        'file': fpath,
                        'line': line_num,
                        'rule': 'Rule 5: KaTeX \\mathbf with Operators',
                        'detail': f"Operator inside \\mathbf: '\\mathbf{{{inner}}}'"
                    })

            # Rule 6: Table cell integrity
            if is_table:
                mc_cleaned = re.sub(r'\\(?:mid|lvert|rvert|vert|lVert|rVert|parallel|\|)', '', math_content)
                if '|' in mc_cleaned:
                    violations.append({
                        'file': fpath,
                        'line': line_num,
                        'rule': 'Rule 6: Table Cell Integrity',
                        'detail': f"Raw '|' in table inline math: '{full_match}'"
                    })

            # Rule 7: Asterisk escapes
            if re.search(r'(?<!\\)\*', math_content):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 7: Math Asterisk Escapes',
                    'detail': f"Raw '*' in inline math: '{full_match}'"
                })

            # Rule 13: Lie Group & Matrix Group Underscore Subscripts & Extensible Font Accents
            if re.search(r'\\(?:widetilde\{\\mathrm\{SL\}\}|tilde\{\\mathrm\{SL\}\}|mathrm\{SL\}|mathrm\{PSL\}|mathfrak\{sl\}|mathrm\{GL\}|mathrm\{SO\}|mathrm\{SU\})_\{?[0-9a-zA-Z]+\}?', math_content):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 13: Lie Group Underscore Subscript',
                    'detail': f"Lie group with underscore subscript (causes CommonMark emphasis collision, use parameter syntax like '\\mathrm{{SL}}(2, \\mathbb{{R}})' or '\\mathrm{{SU}}(2)'): '{full_match}'"
                })

            # Rule 14: Parenthesis Delimiter Nesting Collision
            if re.search(r'\(\$(?:[^\$\n]*[\(\)][^\$\n]*)\$\)', line):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 14: Parenthesis Delimiter Collision',
                    'detail': f"Outer '($...$)' wrapping math with internal parentheses creates delimiter parser collision. Move parentheses inside math '$ ( ... ) $': '{line.strip()}'"
                })

            # Rule 15: Scalable curly delimiter unescaped
            if re.search(r'\\left\\\{|\\right\\\}', math_content):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 15: Unescaped Scalable Curly Delimiters',
                    'detail': f"Found '\\left\\{{' or '\\right\\}}' in inline math (use '\\left\\lbrace' / '\\right\\rbrace'): '{full_match}'"
                })

            # Rule 16: Brace-preceded font macro subscripts (\mathcal{H}_F, \mathbb{Q}_p)
            if re.search(r'\\(?:mathcal|mathbb|mathbf|mathfrak|vec)\{[A-Za-z]\}_[0-9a-zA-Z]', math_content):
                violations.append({
                    'file': fpath,
                    'line': line_num,
                    'rule': 'Rule 16: Font Macro Brace-Subscript Collision',
                    'detail': f"Brace-preceded font macro subscript creates left-flanking delimiter run (drop braces for intraword immunity, e.g. '\\mathbb Q_p' or '\\mathcal H_F'): '{full_match}'"
                })

    return violations

def main():
    targets = sys.argv[1:]
    if not targets:
        default_files = [
            r'c:\Users\x\Documents\antigravity\original-research\papers\paper2_cosmology.md',
            r'c:\Users\x\Documents\antigravity\original-research\papers\paper1_spectral_geometry.md',
            r'c:\Users\x\Documents\antigravity\original-research\README.md',
        ]
        targets = [f for f in default_files if os.path.isfile(f)]
        if not targets:
            print("Usage: python audit_gfm_math.py <path/to/markdown1.md> [path/to/markdown2.md ...]")
            sys.exit(0)

    print("=======================================================")
    print("      RUNNING STRICT GFM MATH VERIFICATION AUDIT       ")
    print("=======================================================")
    total = 0
    for f in targets:
        v = run_strict_linter(f)
        total += len(v)
        status = "[PASS: 0 VIOLATIONS]" if len(v) == 0 else f"[FAIL: {len(v)} VIOLATIONS]"
        print(f"{status} - {os.path.basename(f)} ({f})")
        for item in v:
            print(f"  Line {item['line']:4d} | [{item['rule']}] {item['detail']}")

    print("\n=======================================================")
    print(f"FINAL AUDIT RESULT: {total} TOTAL VIOLATIONS")
    print("=======================================================")
    if total != 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
