# Copyright 2024 Axel Walthelm
import os
import re
from collections import defaultdict


debug_print = False
current_filename = None

def tree_contains_at_least_n_items(tree, n):
	def count_down_items(obj, n):
		#if debug_print: print("obj,n:", (obj, n))
		if isinstance(obj, (list, tuple)) and len(obj) > 0:  # empty list/tuple counts as an item
			for item in obj:
				n = count_down_items(item, n)
				if n <= 0:
					break
			#if debug_print: print("n out:", n)
			return n
		else:
			#if debug_print: print("n-1 out:", n-1)
			return n - 1

	return count_down_items(tree, n) <= 0


def print_tree(tree, indent=0, name=None):
	if name:
		print("tree:", name)

	def print_indent(indent):
		print(" " * indent, end='')

	def print_comma_newline(indent):
		print(",")  # newline
		print_indent(indent)

	def print_comma_space():
		print(", ", end='')

	# invariant: output stream is indented before and after call; returns without printing final newline
	def print_tree_recurse(tree, indent):
		print("[", end='')
		is_last_tree = False
		for i, v in enumerate(tree):
			if isinstance(v, list) and tree_contains_at_least_n_items(v, 2):
				if i > 0:
					print_comma_newline(indent + 1)
				print_tree_recurse(v, indent + 1)
				is_last_tree = True
			else:
				if i > 0:
					if is_last_tree:
						print_comma_newline(indent + 1)
					else:
						print_comma_space()
				print(repr(v), end='')
				is_last_tree = False
		print("]", end='')

	print_indent(indent)
	print_tree_recurse(tree, indent)
	print()


def clean_tree(tree):
	if len(tree) == 1 and isinstance(tree[0], list):
		tree[:] = tree[0]  # remove redundant level (at root)
	for i, item in enumerate(tree):
		if isinstance(item, list) and len(item) == 1:
			tree[i] = item[0]  # remove redundant level (at leaf)
	for item in tree:
		if isinstance(item, list):
			clean_tree(item)


ppoperators = (
	# operator, precedence, left-to-right, arguments before, arguments after, function
	# Parentheses '(' and ')' are not operators, but have highest precedence.
	# For ternary operator '?:' the expression in the middle of the conditional operator (between '?' and ':')
	# is parsed as if parenthesized: its precedence relative to ?: is ignored.
	# We do top-down short-circuit evaluation. Therefore each function in this table
	# gets each arguments as a term (a tree of operations) and a term evaluation function e as its last argument.
	# Function e() always returns int. Each function must return int or bool.
	('+\1', 2, False, 0, 1, lambda x, e: +e(x)),
	('-\1', 2, False, 0, 1, lambda x, e: -e(x)),
	('!',   2, False, 0, 1, lambda x, e: not e(x)),
	('~',   2, False, 0, 1, lambda x, e: ~e(x)),
	('*',   3,  True, 1, 1, lambda x, y, e: e(x) * e(y)),
	('/',   3,  True, 1, 1, lambda x, y, e: e(x) // e(y)),
	('%',   3,  True, 1, 1, lambda x, y, e: e(x) % e(y)),
	('+',   4,  True, 1, 1, lambda x, y, e: e(x) + e(y)),
	('-',   4,  True, 1, 1, lambda x, y, e: e(x) - e(y)),
	('<<',  5,  True, 1, 1, lambda x, y, e: e(x) << e(y)),
	('>>',  5,  True, 1, 1, lambda x, y, e: e(x) >> e(y)),
	('<',   6,  True, 1, 1, lambda x, y, e: e(x) < e(y)),
	('<=',  6,  True, 1, 1, lambda x, y, e: e(x) <= e(y)),
	('>',   6,  True, 1, 1, lambda x, y, e: e(x) > e(y)),
	('>=',  6,  True, 1, 1, lambda x, y, e: e(x) >= e(y)),
	('==',  7,  True, 1, 1, lambda x, y, e: e(x) == e(y)),
	('!=',  7,  True, 1, 1, lambda x, y, e: e(x) != e(y)),
	('&',   8,  True, 1, 1, lambda x, y, e: e(x) & e(y)),
	('^',   9,  True, 1, 1, lambda x, y, e: e(x) ^ e(y)),
	('|',  10,  True, 1, 1, lambda x, y, e: e(x) | e(y)),
	('&&', 11,  True, 1, 1, lambda x, y, e: bool(e(x) and e(y))),
	('||', 12,  True, 1, 1, lambda x, y, e: bool(e(x) or e(y))),
	(':',  13, False, 1, 1, lambda x, y, e: ternary_evaluation(':', x, y, e)),
	('?',  13, False, 1, 1, lambda x, y, e: ternary_evaluation('?', x, y, e)),
)

def ternary_evaluation(op, x, y, e):
	# since we do top-down short-circuit evaluation, the ':' operator should never be evaluated
	if op == ':':
		raise SyntaxError(": without matching ?")

	assert(op == '?')
	if y[0] != ':':
		raise SyntaxError("? without matching :")

	return e(y[1]) if e(x) else e(y[2])


ppoperators_of_precedence = defaultdict(list)
for op in ppoperators:
	ppoperators_of_precedence[op[1]].append(op[0])

ppoperator_of_names = {op[0]: op for op in ppoperators}


re_operator = r"|".join(re.escape(op) for op in sorted([t[0] for t in ppoperators if ord(t[0][-1]) != 1], reverse=True))  # sort because first regex alternative is matched, not longest
re_parenthesis = r"[()]"
re_value = r"\w+"  # TODO: r"\.?\d(?:[eEpP][+-]|[\w.])*"

identifier_re = re.compile(r"^[a-zA-Z_]\w*$")

# regular expression to split string into first token (operator, parenthesis, or value) and the rest
first_token_re = re.compile(r"^\s*(?:({})|({})|({}))(.*)$".format(re_operator, re_parenthesis, re_value))


def tokenize_term(term):
	tokens = []
	last_token_indicates_unary = True
	while term.strip():
		m = first_token_re.match(term)
		if not m:
			raise SyntaxError("invalid token at start of " + repr(term))
		operator, parenthesis, value, term = m.groups()
		if parenthesis or value:
			tokens.append(parenthesis or value)
		else:
			if operator in ('+', '-') and last_token_indicates_unary:
				tokens.append(operator + "\1")  # disambiguate unary and binary '+' and '-'
			elif operator in ('?', ':'):
				tokens += ['?', '('] if operator == '?' else [')', ':']  # parenthesize ternary operator
			else:
				tokens.append(operator) # normal operator

		last_token_indicates_unary = operator or parenthesis == '('

	return tokens


# use parentheses to turn list of tokens into tree
def parenthesize(tokens):
	tree = []
	while tokens:
		token = tokens.pop(0)
		if token == "(":
			subtree, closing_token, tokens = parenthesize(tokens)
			assert closing_token == ")", "unmatched opening parenthesis"
			tree.append(subtree)
		elif token == ")":
			return tree, token, tokens
		else:
			tree.append(token)
	return tree, None, []


# tranform parenthesized tree according to operator precedence
def apply_operator_precedence(tree):
	# process sub-trees, i.e. terms in parentheses
	for pos in range(len(tree)):
		if isinstance(tree[pos], list):
			apply_operator_precedence(tree[pos])

	# process top level of tree
	for precedence in sorted(ppoperators_of_precedence.keys()):
		operator_names = ppoperators_of_precedence[precedence]
		is_left_to_right = ppoperator_of_names[operator_names[0]][2]
		assert all(is_left_to_right == ppoperator_of_names[n][2] for n in operator_names)
		while True:
			if is_left_to_right:
				pos = next((i for i, n in enumerate(tree) if n in operator_names), -1)
			else:
				pos = next((i for i, n in reversed(list(enumerate(tree))) if n in operator_names), -1)
			if pos < 0:
				break

			operator, _, _, arguments_before, arguments_after, _ = ppoperator_of_names[tree[pos]]

			operation = [operator] + tree[pos - arguments_before: pos] + tree[pos + 1: pos + arguments_after + 1]
			assert len(operation) == arguments_before + 1 + arguments_after, "missing arguments for operator " + operator
			if debug_print: print("operation", operation)

			# push operation down one level
			tree[pos - arguments_before: pos + arguments_after + 1] = [operation]
			if debug_print: print(tree)


# evaluate expression tree (built by parenthesize and apply_operator_precedence) using operator functions
def eval_expression(expression):
	if debug_print: print("eval_expression in: {}".format(expression))

	if isinstance(expression, list) and len(expression) == 1:
		return eval_expression(expression[0])  # ignore redundant levels (e.g. from parentheses)

	if isinstance(expression, str):
		# convert integer literals to int and undefined symbols to 0
		m = re.match(r"^\s*[+|-]?(?:0[xX][\dA-Fa-f]+|0[bB][01]+|(0)\d+|\d+)\s*$", expression)
		if m:
			if m.groups()[0]:
				value = int(expression, 8)  # numbers starting with 0 are octal
			else:
				value = int(expression, 0)  # decimal, hex '0x', binary '0b' - unlike C/C++ underscore as separator is allowed
		elif identifier_re.match(expression.strip()):
			value = 0  # unknown identifier have value zero
		else:
			raise SyntaxError("Invalid expression: " + repr(expression))

		if debug_print: print("eval_expression val out: {} => {}".format(expression, value))
		return value

	try:
		operator, precedence, is_left_to_right, arguments_before, arguments_after, function = next(t for t in ppoperators if t[0] == expression[0])
	except:
		raise SyntaxError("Invalid expression: " + repr(expression))
	if debug_print: print("eval_expression op: {}".format(operator))
	value = function(*expression[1:], eval_expression)
	if debug_print: print("eval_expression op out: {} => {}".format(expression, value))
	assert isinstance(value, (int, bool))
	return int(value)  # all operations return integer numbers


def substitute_preserve_spaces(term, ppvars):
	# In contrast to standard C/C++ preprocessor variable substitution, this function preserves spaces as is.
	# It does not insert spaces between replaced 'tokens' and it does not implement a token pasting operator ##,
	# but if the value of a variable in ppvars starts or ends with ##, spaces to the left or right are removed
	# as well as the ##. A value of "##" starts and ends with ##, so it acts similar to the ## operator.
	# Hint: that the standard C/C++ preprocessor supports ## (and stringification operator #) only during macro expansion.
	# Note: since variable names are words, we substitute only at word boundaries. With that,
	# the order of substitution is irrelevant, as long as we avoid substituting already substituted parts again.
	def substitute(term):
		for var in ppvars:
			m = re.match(r"^(.*)\b{}\b(.*)$".format(var), term)
			if m:
				before, after = m.groups()
				before, middle, after = substitute(before), ppvars[var], substitute(after)
				if middle in ("##", "###"):  # TBD: is there a special meaning of "###"?
					return before.rstrip() + after.lstrip()
				if middle[0:2] == "##":
					before, middle = before.rstrip(), middle[2:]
				if middle[-2:] == "##":
					middle, after = middle[:-2], after.lstrip()
				return before + middle + after
		return term

	return substitute(term)


def evaluate_condition(term, ppvars):
	try:
		if debug_print: print('term:', term)
		if debug_print: print('ppvars:', ppvars)

		for v in ppvars:
			assert isinstance(v, str), "Preprocessor variable names must be strings. {}: {}".format(repr(v), repr(ppvars[v]))
			assert identifier_re.match(v), "Preprocessor variable names must be valid identifier. {}: {}".format(repr(v), repr(ppvars[v]))
			assert isinstance(ppvars[v], str), "Preprocessor variable values must be strings. {}: {}".format(repr(v), repr(ppvars[v]))

		term = re.sub(r"/\*.*?\*/", " ", term).strip()  # remove block comments - TODO: do not remove comments in string literals
		term = re.sub(r"\s*//.*$", " ", term)  # remove end comment

		# Evaluate 'defined(ID)' and 'defined ID' on term string,
		# because it needs to be evaluated before substituting ppvars.
		while True:
			m = re.match(r"^(.*)\bdefined\s*(?:\b(\w+)|\(([^)]*)\))\s*(.*)$", term)
			if not m:
				break

			before, var1, var2, after = m.groups()
			var = (var1 or var2).strip()
			if not identifier_re.match(var):
				raise SyntaxError("expected 'defined(ID)'")

			term = before.rstrip() + (" 1 " if var in ppvars else " 0 ") + after.lstrip()

		if debug_print: print('term defined:', term)

		# Substitute ppvars in term before tokenization, because values in ppvars may contain multiple tokens.
		# Note: comments are expected to already be removed.
		term = substitute_preserve_spaces(term, ppvars)

		if debug_print: print('term subst.:', term)

		# Split into tokens.
		tokens = tokenize_term(term)

		if debug_print: print('tokens:', tokens)

		# Evaluate round braces.
		tree, closing_token, extra_tokens = parenthesize(tokens)
		if debug_print: print_tree(tree, name="parenthesize")
		clean_tree(tree)
		if debug_print: print_tree(tree, name="parenthesize clean")
		assert closing_token is None, "unmatched closing parenthesis"
		assert not extra_tokens

		# Evaluate order to apply operators.
		apply_operator_precedence(tree)
		if debug_print: print('============================')
		if debug_print: print_tree(tree, name="apply_operator_precedence")
		if debug_print: print('============================')
		clean_tree(tree)
		if debug_print: print('============================')
		if debug_print: print_tree(tree, name="apply_operator_precedence clean")
		if debug_print: print('============================')

		# Calculate result.
		return eval_expression(tree)

	except:
		print("#" * 60)
		print("# FAILED: evaluate_condition(term, ppvars)")
		print("#   term:", repr(term))
		print("#   ppvars:", repr(ppvars))
		print("#" * 60)
		raise


def Process(lines, ppvars, condition_is_relevant=lambda term: True, filename=None):
	global current_filename
	current_filename = filename or "Process-lines"

	try:
		condition_lines = [(li, l) for li, l in enumerate(lines) if re.match(r"^\s*#\s*(ifndef|ifdef|if|elif|else|endif)\b", l)]
		# TODO: line continuation of condition by '\' at end of line or by multi-line block-comment '/*' ... '*/'
		for li, l in condition_lines:
			assert l.rstrip()[-1:] != '\\', "{}({}): line continuation of condition by '\\' not supported.".format(current_filename, li + 1)
			assert not re.search(r"[/][*]((?![*][/]).)*$", l), "{}({}): line continuation of condition by '/*' not supported.".format(current_filename, li + 1)

		# TBD:    Trigraph:  ??(  ??)  ??<  ??>  ??=  ??/  ??'  ??!  ??-
		#      Replacement:   [    ]    {    }    #    \    ^    |    ~
		# TBD:     Digraph:   <%  %>  <:  :>  %:  %:%:
		#       Punctuator:   {   }   [   ]   #    ##

		# collect #if-#elif-#else-#endif lines by nesting level
		condition_lines_of_levels = defaultdict(list)
		level = 0
		for li, line in condition_lines:
			m = re.match(r"^\s*#\s*(\S+)\b\s*(.*)$", line)
			cmd, term = m.groups()
			if cmd == "ifdef":
				cmd, term = "if", "defined " + term
			elif cmd == "ifndef":
				cmd, term = "if", "!defined " + term
			if cmd == "if":
				level += 1
			condition_lines_of_levels[level].append((li, cmd, term))
			if cmd == "endif":
				level -= 1
			assert level >= 0, "{}({}): #endif without #if".format(current_filename, li + 1)

		if debug_print: print("condition_lines_of_levels", condition_lines_of_levels)
		assert level == 0, "{}({}): missing #endif".format(current_filename, len(lines))

		# build groups of corresponding #if-#elif-#else-#endif
		condition_lines_groups = [[]]
		for level in condition_lines_of_levels:
			for li, cmd, term in condition_lines_of_levels[level]:
				# every if starts a new group
				if cmd == "if":
					condition_lines_groups.append([])
				condition_lines_groups[-1].append((li, cmd, term))
			condition_lines_groups.append([])

		if debug_print: print("condition_lines_groups 1:", condition_lines_groups)

		# check groups and remove those that are not relevant
		for gi in reversed(range(len(condition_lines_groups))):
			group = condition_lines_groups[gi]
			if not group:
				del condition_lines_groups[gi]  # empty group is not relevant
				continue

			# check group syntax
			li, cmd, term = group[0]
			assert cmd == "if", "{}({}): missing #if".format(current_filename, li + 1)
			li, cmd, term = group[-1]
			assert cmd == "endif", "{}({}): missing #endif".format(current_filename, li + 1)
			has_else = False
			for i in range(1, len(group) - 1):
				li, cmd, term = group[i]
				assert cmd in ("elif", "else"), "{}({}): invalid #{}".format(current_filename, li + 1, cmd)  # is this already checked above?
				assert not has_else, "{}({}): #{} after #else".format(current_filename, li + 1, cmd)
				if cmd == "else":
					has_else = True

			# check if group is relevant
			is_relevant = any(condition_is_relevant(term) for li, cmd, term in group)
			if not is_relevant:
				del condition_lines_groups[gi]

		if debug_print: print("condition_lines_groups 2:", condition_lines_groups)

		# figure out which lines are to be removed
		to_be_removed = set()
		for group in condition_lines_groups:
			block_was_kept = False  # we can keep only a single code block, all other blocks are removed
			for i in range(len(group) - 1):
				li, cmd, term = group[i]
				keep_block = not block_was_kept and (evaluate_condition(term, ppvars) if cmd != "else" else 1)
				if keep_block:
					to_be_removed.add(li)  # remove #if/#elif/#else
					block_was_kept = True
				else:
					li1, cmd1, term1 = group[i + 1]
					to_be_removed |= set(range(li, li1))  # remove #if/#elif/#else with block
			li, cmd, term = group[-1]
			to_be_removed.add(li)  # remove #endif

		# remove lines
		for i in sorted((i for i in to_be_removed), reverse=True):
			del lines[i]

		# apply pre-pre-processing variables
		for line_index, line in enumerate(lines):
			line = substitute_preserve_spaces(line, ppvars)
			lines[line_index] = line

		if debug_print:
			print('v' * 60)
			for line in lines:
				print(repr(line))
			print('^' * 60)

	except:
		print("#" * 60)
		print("# FAILED: evaluate_condition(term, ppvars)")
		print("#   lines:", len(lines))
		print("#   filename:", repr(current_filename))
		print("#   ppvars:", repr(ppvars))
		print("#" * 60)
		raise


if __name__ == "__main__":
	print("Testing...")
	debug_print = True

	def assert_throw(function):
		assert callable(function)
		did_throw = False
		try:
			function()
		except Exception as ex:
			print(ex)
			print("NOTE: EXCEPTION WAS EXPECTED BY TEST.")
			print()
			did_throw = True
		assert did_throw

	lines = ["hat\f VAR  ck"];
	Process(lines, {'VAR': '\ttri\v'})
	assert lines == ["hat\f \ttri\v  ck"]

	lines = ["hat VAR ch"];
	Process(lines, {'VAR': '##'})
	assert lines == ["hatch"]

	lines = ["hat\t  VAR \fck"];
	Process(lines, {'VAR': '## tri'})
	assert lines == ["hat tri \fck"]

	lines = ["hat\t  VAR \fck"];
	Process(lines, {'VAR': 'tri##'})
	assert lines == ["hat\t  trick"]

	lines = ["hat\t  VAR \fck"];
	Process(lines, {'VAR': '##tri##'})
	assert lines == ["hattrick"]

	Process(["#if 1", "#endif"], {})  # this does not throw
	assert_throw(lambda: Process(["#if 1\\", "#endif"], {}))  # line continuation of condition by '\' not supported.
	assert_throw(lambda: Process(["#if 1 \\ ", "#endif"], {}))  # line continuation of condition by '\' not supported.
	assert_throw(lambda: Process(["#if 1 /* multi line comment", "#endif"], {}))  # line continuation of condition by '/*' not supported.

	lines_orig = [
		"#if X     //o__#1__o//",
		"  fX+X+defined(X)",
		"#elif Y   //o__#2__o//",
		"  fY/Y+defined(Y)",
		"#else     //o__#3__o//",
		"  fZ(Z)+defined(Z)",
		"#endif    //o__#4__o//",
		]

	lines = list(lines_orig)
	Process(lines, {'X': '0', 'Y': '1'})
	assert lines == ['  fY/1+defined(1)']

	lines = list(lines_orig)
	Process(lines, {'X': '0', 'Y': '0'})
	assert lines == ['  fZ(Z)+defined(Z)']

	lines = list(lines_orig)
	Process(lines, {'X': '1', 'Y': '0'})
	assert lines == ['  fX+1+defined(1)']

	for marker in ("//o__#{}__o//".format(i) for i in (1, 2, 3, 4)):
		lines = list(lines_orig)
		Process(lines , {'X': '0', 'Y': '1'}, lambda t: marker in t)
		assert lines == ['  fY/1+defined(1)']

	lines = list(lines_orig)
	Process(lines, {'X': '0', 'Y': '1'}, lambda t: "//o__#__o//" in t)
	assert len(lines) == len(lines_orig)

	lines_orig = [
		"#if X //o__#__o//",
		"# if X",
		"  fX+X+defined(X)",
		" #endif // X",
		"#elif Y",
		"  fY/Y+defined(Y)",
		"#else",
		"  fZ(Z)+defined(Z)",
		"#endif",
		]

	lines = list(lines_orig)
	Process(lines, {'X': '1', 'Y': '0'},  lambda t: "//o__#__o//" in t)
	assert lines == [
		"# if 1",
		"  fX+1+defined(1)",
		" #endif // 1"]


	assert  1 == evaluate_condition("defined X", {"X": '+'})
	assert  1 == evaluate_condition("defined(X)", {"X": '+'})
	assert  1 == evaluate_condition(" defined ( X ) ", {"X": '+'})
	assert  0 == evaluate_condition("defined Y", {"X": '+'})

	assert  7 == evaluate_condition("X", {"X": '7'})
	assert  7 == evaluate_condition("X", {"X": '+7'})
	assert -7 == evaluate_condition("X", {"X": '-7'})
	assert  7 == evaluate_condition("X", {"X": '0x7'})
	assert  7 == evaluate_condition("X", {"X": '+0x07'})
	assert -7 == evaluate_condition("X", {"X": '-0x00007'})
	assert  7 == evaluate_condition("X", {"X": '07'})
	assert 15 == evaluate_condition("X", {"X": '017'})
	assert  6 == evaluate_condition("X", {"X": '0b00000110'})

	assert_throw(lambda: 1000 == evaluate_condition("X", {"X": '1_000'}))  # ok in Python, not ok for us
	assert_throw(lambda: 17 == evaluate_condition("X", {"X": '0_017'}))  # not ok

	assert  7 == evaluate_condition("X 7", {"X": '+'})
	assert -7 == evaluate_condition("X 7", {"X": '-'})
	assert  0 == evaluate_condition("X 7", {"X": '!'})
	assert -8 == evaluate_condition("X 7", {"X": '~'})
	assert 21 == evaluate_condition("7 X 3", {"X": '*'})
	assert  2 == evaluate_condition("7 X 3", {"X": '/'})
	assert  1 == evaluate_condition("7 X 3", {"X": '%'})
	assert 10 == evaluate_condition("7 X 3", {"X": '+'})
	assert  4 == evaluate_condition("7 X 3", {"X": '-'})
	assert 14 == evaluate_condition("7 X 1", {"X": '<<'})
	assert  3 == evaluate_condition("7 X 1", {"X": '>>'})
	assert  0 == evaluate_condition("7 X 3", {"X": '<'})
	assert  0 == evaluate_condition("7 X 3", {"X": '<='})
	assert  1 == evaluate_condition("7 X 3", {"X": '>'})
	assert  1 == evaluate_condition("7 X 3", {"X": '>='})
	assert  0 == evaluate_condition("7 X 3", {"X": '=='})
	assert  1 == evaluate_condition("7 X 3", {"X": '!='})
	assert  3 == evaluate_condition("7 X 3", {"X": '&'})
	assert  4 == evaluate_condition("7 X 3", {"X": '^'})
	assert  7 == evaluate_condition("6 X 3", {"X": '|'})
	assert  1 == evaluate_condition("7 X 3", {"X": '&&'})
	assert  1 == evaluate_condition("7 X 3", {"X": '||'})
	assert  3 == evaluate_condition("7 X 3:4", {"X": '?'})

	assert 0 == evaluate_condition("((1)*5+adsf+defined(X)) ? a+c : b", {"X": '666'})

	assert 1 == evaluate_condition("1+defined(o__XX__o) || defined X / 0", {"X": '666'})

	assert 0 == evaluate_condition("-!defined(X) && 1/0", {"X": '666'})

	assert 16 == evaluate_condition("X << 4", {"X": '1'})

	assert -5 == evaluate_condition("~X", {"X": '4'})

	assert 6 == evaluate_condition(" XYZ\t\t\v%     \t7 ", {"XYZ": '13'})

	assert 5 == evaluate_condition("o__X__o  &  7", {"o__X__o": '13'})

	assert -3 == evaluate_condition("X?a+b:0%0", {"X": '666', "a": '+4', "b": '-7'})

	assert '1' == str(evaluate_condition("2 X 3 == 5", {"X": '+'}))

	assert  8 == evaluate_condition("7 X 3", {"X": '+ +4 -'})

	print("... Testing Done.")
