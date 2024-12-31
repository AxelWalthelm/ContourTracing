import os
import sys
import re
from stat import S_IREAD, S_IWUSR
import Preprocessor

directory = os.path.dirname(os.path.abspath(__file__))
template_file = os.path.join(directory, "Template.hpp")
output_file_of_variants = {
	'bool': 'ContourTracing.hpp',
	'thresh': 'ContourTracingThresh.hpp',
	'bitonal': 'ContourTracingBitonal.hpp',
}

variant = (sys.argv[1:2] or ['bool'])[0]
out_folder = (sys.argv[2:3] or ['.'])[0]
if len(sys.argv) not in (2, 3) or variant not in ('bool', 'thresh', 'bitonal'):
	print("Usage: ContourTracingGenerator.py variant [out-folder]")
	print("Possible values for variant are:")
	print("  bool: input image is 1 byte per pixel and 0 is background; compatible with OpenCV")
	print("  thresh: input image is 1 byte per pixel and values <= threshold are background; compatible with OpenCV")
	print("  bitonal: input image is 1 bit per pixel and 0 is background")
	exit(-1)

output_file = os.path.abspath(os.path.join(directory, out_folder, output_file_of_variants[variant]))

# pre-pre-preprocessing variables
ppvars = {
	'o__ONE_BYTE_PER_PIXEL__o': '1' if variant != 'bitonal' else '0',
	'o__ONE_BIT_PER_PIXEL__o': '1' if variant == 'bitonal' else '0',
	'o__THRESHOLD_IS_USED__o': '1' if variant == 'thresh' else '0',
	'o__THRESHOLD_PARAMETER__o': "##, const int threshold" if variant == 'thresh' else "##",
	'o__IMAGE_PARAMETER__o': "##, const uint8_t* const image, const int width, const int height, const int stride" + (
		                     ", const int threshold" if variant == 'thresh' else ""),
	'o__IMAGE_ARGUMENTS__o': "##, image, width, height, stride" + (
		                     ", threshold" if variant == 'thresh' else ""),
}
ppvars['o__IMAGE_PTR_ARGUMENTS__o'] = re.sub(r"\bimage\b", "image_ptr", ppvars['o__IMAGE_ARGUMENTS__o'])

print('ContourTracingGenerator.py -> {}'.format(output_file))

namespace = "FECTS"
namespace_ = namespace + "_"
if variant == 'thresh':
	namespace += "_T"
if variant == 'bitonal':
	namespace += "_B"

def strip_parentheses(term):
	term = term.strip()
	if term[0] == '(' and term[-1] == ')':
		term = term[1, -1].strip()
	return term

def add_parentheses(term):
	return "({})".format(term)

def check_parentheses(term):
	term = strip_parentheses(term)
	if re.search(r"\W", term):  # anything more complicated than a single variable?
		term = add_parentheses(term)
	return term

warning_generated_code = """
#############################################################################
# WARNING: this code was generated - do not edit, your changes may get lost #
#############################################################################
Consider to edit {} and {} instead.
""".strip('\n').format(
		os.path.relpath(__file__,      os.path.dirname(output_file)),
		os.path.relpath(template_file, os.path.dirname(output_file)))

introduction = """
 Fast Edge-Based Contour Tracing from Seed-Point (FECTS)
============================================================

See README.md at https://github.com/AxelWalthelm/ContourTracing/ for more information.

    Definition of direction
                                    x    
    +---------------------------------->  
    |               (0, -1)               
    |                  0  up              
    |                  ^                  
    |                  |                  
    |                  |                  
    | (-1, 0) 3 <------+------> 1 (1, 0)  
    |       left       |      right       
    |                  |                  
    |                  v                  
    |                  2  down            
  y |               (0, 1)                
    v                                     
""".strip('\n')

def vector(dir):
	return ((0, -1), (1, 0), (0, 1), (-1, 0))[dir]

def turn_left(dir, clockwise):
	return (3, 0, 1, 2)[dir] if clockwise else (1, 2, 3, 0)[dir]

def turn_right(dir, clockwise):
	return (1, 2, 3, 0)[dir] if clockwise else (3, 0, 1, 2)[dir]

def is_forward_border_code(dir):
	return ('y == 0', 'x == width_m1', 'y == height_m1', 'x == 0')[dir]

def is_not_forward_border_code(dir):
	return ('y != 0', 'x != width_m1', 'y != height_m1', 'x != 0')[dir]

def is_forward_border_no_m1_code(dir):
	return is_forward_border_code(dir).replace("_m1", " - 1")

def is_left_not_border_code(dir, clockwise):
	return is_not_forward_border_code(turn_left(dir, clockwise))

def forward_vector(dir, clockwise):
	return vector(dir)

def forward_left_vector(dir, clockwise):
	v1 = vector(dir)
	v2 = vector(turn_left(dir, clockwise))
	return (v1[0] + v2[0], v1[1] + v2[1])

def forward_pixel(dir, clockwise):
	return offset_pixel(forward_vector(dir, clockwise))

def forward_left_pixel(dir, clockwise):
	return offset_pixel(forward_left_vector(dir, clockwise))

def pixel_off_code(vec):
	sign = {-1: 'm', 0: '0', 1: 'p'}
	return "off_" + sign[vec[0]] + sign[vec[1]]

def is_value_foreground_code(value_code):
	if variant == 'bitonal':
		m = re.match(r"^\s*(\w+)\s*\[(.*)\]\s*$", value_code)
		array_pointer, array_index = m.groups()
		if array_pointer == 'image':
			return "bittest(image, {})".format(array_index)
		return "bittest(image, {} + {})".format(check_parentheses(array_pointer), check_parentheses(array_index))

	if variant == 'thresh':
		return "{} > threshold".format(value_code)

	assert variant == 'bool'
	return "{} != 0".format(value_code)

def is_pixel_foreground_code(vec):
	return is_value_foreground_code("pixel[{}]".format(pixel_off_code(vec)))

def move_pixel_code_lines(vec, indent):
	lines = []
	lines.append("pixel += {};".format(pixel_off_code(vec)))
	def inc(var_name, delta):
		assert delta in (-1, 0, 1)
		if delta == 1:
			lines.append("++{};".format(var_name))
		elif delta == -1:
			lines.append("--{};".format(var_name))

	inc('x', vec[0])
	inc('y', vec[1])
	return [indent + l for l in lines]


def mirror_rule(rule):
	pixel, xy, inner, outer, right, emit = rule
	def mirror(r):
		return tuple(r[i] for i in (1, 0, 3, 2))

	def mirror_arrows(r):
		map = {'<': '>', '>': '<', 'v': 'v', '^': '^', '|': '|', '-': '-'}
		return tuple(map[r[i]] for i in (0, 3, 2, 1))

	map = {'left': 'right', 'right': 'left', '': ''}
	return (mirror(pixel), mirror(xy), mirror_arrows(inner), mirror_arrows(outer), map[right], emit)


def rotate_rule_clockwise(rule):
	pixel, xy, inner, outer, right, emit = rule
	def rotate(r):
		return tuple(r[i] for i in (3, 0, 1, 2))

	def rotate_arrows(r):
		map = {'^': '>', '>': 'v', 'v': '<', '<': '^', '|': '-', '-': '|'}
		return tuple(map[v] for v in rotate(r))

	return (rotate(pixel), rotate(xy), rotate_arrows(inner), rotate_arrows(outer), right, emit)


def make_rule_ascii_art(number, rule, dir, clockwise):
	if not clockwise:
		rule = mirror_rule(rule)
	for _ in range(dir):
		rule = rotate_rule_clockwise(rule)
	pixel, xy, inner, outer, right, emit = rule
	lines = []
	lines.append('rule {}:'.format(number))
	lines.append('+-------+-------+    ')
	lines.append('|{: ^7}{}{: ^7}|    '.format('', outer[0], ''))
	lines.append('|{: ^7}{}{: ^7}|    '.format(pixel[0], '|', pixel[1]))
	lines.append('|{: ^7}{}{: ^7}|    '.format(xy[0], inner[0], xy[1]))
	lines.append('+{}-----{}+{}-----{}+    '.format(outer[3], inner[3], inner[1], outer[1]))
	lines.append('|{: ^7}{}{: ^7}|    '.format('', inner[2], ''))
	lines.append('|{: ^7}{}{: ^7}|    '.format(pixel[3], '|', pixel[2]))
	lines.append('|{: ^7}{}{: ^7}|    '.format(xy[3], outer[2], xy[2]))
	lines.append('+-------+-------+    ')
	lines.append('=> turn {}'.format(right) if right else '=> move ahead')
	lines.append('=> emit pixel (x,y)' if emit else '')
	maxlen = max(len(l) for l in lines)
	return [(l + (' ' * maxlen))[:maxlen] for l in lines]

def make_rules_ascii_art(dir, clockwise, prefix):
	is_proto_rule = dir == 0 and clockwise
	t = 'direction {} {} rules{}:'.format(dir,
			'clockwise' if clockwise else 'counterclockwise',
			' with border checks' if is_proto_rule else '')
	title = '{0}{1}\n{0}{2}\n\n'.format(prefix, t, '=' * len(t))

	if is_proto_rule:
		basic_rules_0cw = """
direction 0 basic clockwise rules:
==================================

                     rule 1:              rule 2:              rule 3:              
                     +-------+-------+    +-------+-------+    +-------+-------+    
                     |       |       |    |       ^       |    |       |       |    1: foreground
                     |   1   |  0/1  |    |   0   |   1   |    |   0   |   0   |    0: background or border
                     |  ???  |       |    |       |  ???  |    |       |  ???  |    /: alternative
                     +<------+-------+    +-------+-------+    +-------+------>+    
                     |       ^       |    |       ^       |    |       ^       |    (x,y): current pixel
                     |   0   |   1   |    |   0   |   1   |    |   0   |   1   |    ???: pixel to be checked
                     |       | (x,y) |    |       | (x,y) |    |       | (x,y) |    
                     +-------+-------+    +-------+-------+    +-------+-------+    
                     - turn left          - move ahead         - turn right
                     - emit pixel (x,y)   - emit pixel (x,y)
"""
		title = '\n'.join(prefix + l for l in basic_rules_0cw.splitlines()[1:]) + '\n\n\n' + title


	_ = """
direction 0 clockwise rules with border checks:

rule 0:              rule 1:              rule 2:              rule 3:              
+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+    
|       |       |    |       |       |    |       ^       |    |       |       |    1: foreground
|   b   |   b   |    |   1   |  0/1  |    |  0/b  |   1   |    |  0/b  |   0   |    0: background
|       |       |    |       |       |    |       |       |    |       |       |    b: border outside of image
+-------+------>+    +<------+-------+    +-------+-------+    +-------+------>+    /: alternative
|       ^       |    |       ^       |    |       ^       |    |       ^       |    
|  0/b  |   1   |    |   0   |   1   |    |  0/b  |   1   |    |  0/b  |   1   |    
|       | (x,y) |    |       | (x,y) |    |       | (x,y) |    |       | (x,y) |    
+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+    
=> turn right        => turn left         => move ahead        => turn right
                     => emit pixel (x,y)  => emit pixel (x,y)
"""
	rule0 = (('b', 'b', '1', '0/b'), ('???', '???', '(x,y)', ''), ('|', '-', '^', '-'), ('|', '>', '|', '-'), 'right', '')
	rule1 = (('1', '0/1', '1', '0'), ('???', '', '(x,y)', ''), ('|', '-', '^', '-'), ('|', '-', '|', '<'), 'left', 'emit')
	rule2 = (('0/b', '1', '1', '0/b'), ('', '???', '(x,y)', ''), ('|', '-', '^', '-'), ('^', '-', '|', '-'), '', 'emit')
	rule3 = (('0/b', '0', '1', '0/b'), ('', '???', '(x,y)', ''), ('|', '-', '^', '-'), ('|', '>', '|', '-'), 'right', '')
	ascii_art_rules = [rule0, rule1, rule2, rule3]

	blocks = [make_rule_ascii_art(i, rule, dir, clockwise) for i, rule in enumerate(ascii_art_rules)]
	legend = [
	 '',
	 '',
	 '1: foreground',
	 '0: background',
	 'b: border outside of image',
	 '/: alternative',
	 '',
	 '(x,y): current pixel',
	 '???: pixel to be checked',
	 '',
	 '',
	 '']
	blocks.append(legend)
	maxlines = max(len(b) for b in blocks)
	lines = [prefix + ''.join(b[l] for b in blocks).rstrip() for l in range(maxlines)]
	return title + '\n'.join(lines);

def make_rules_code(dir, clockwise, indent):
	left = 'left' if clockwise else 'right'
	right = 'right' if clockwise else 'left'

	lines = []
	lines.append('// if forward is border (rule 0)')
	lines.append('if ({})'.format(is_forward_border_code(dir)))
	lines.append('{')
	lines.append('    // turn {}'.format(right))
	new_dir = turn_right(dir, clockwise)
	lines.append('    dir = {};'.format(new_dir))
	if (sorted([dir, new_dir]) == [0, 3]):
		lines.append('    {}sum_of_turn_overflows;'.format('++' if (new_dir < dir) == clockwise else '--'))
	#lines.append('    ++sum_of_turns;')
	lines.append('}')
	lines.append('// else if {0} is not border and forward-{0} pixel is foreground (rule 1)'.format(left))
	lines.append('else if ({} && {})'.format(is_left_not_border_code(dir, clockwise),
											 is_pixel_foreground_code(forward_left_vector(dir, clockwise))))
	lines.append('{')
	lines.append('    // emit current pixel')
	lines.append('    contour.emplace_back(x, y);')
	lines.append('    // go to checked pixel');
	lines += move_pixel_code_lines(forward_left_vector(dir, clockwise), '    ');
	lines.append('    // turn {}'.format(left));
	new_dir = turn_left(dir, clockwise)
	lines.append('    dir = {};'.format(new_dir))
	#lines.append('    --sum_of_turns;')
	if (sorted([dir, new_dir]) == [0, 3]):
		lines.append('    {}sum_of_turn_overflows;'.format('++' if (new_dir < dir) == clockwise else '--'))
	lines.append('    // stop if buffer is full')
	lines.append('    if (++contour_length >= max_contour_length)')
	lines.append('        break;')
	lines.append('    // set pixel valid');
	lines.append('    is_pixel_valid = true;');
	lines.append('}')
	lines.append('// else if forward pixel is foreground (rule 2)')
	lines.append('else if ({})'.format(is_pixel_foreground_code(forward_vector(dir, clockwise))))
	lines.append('{')
	lines.append('    // if pixel is valid')
	lines.append('    if (is_pixel_valid)')
	lines.append('    {')
	lines.append('        // emit current pixel')
	lines.append('        contour.emplace_back(x, y);')
	lines.append('    }')
	lines.append('    // go to checked pixel');
	lines += move_pixel_code_lines(forward_vector(dir, clockwise), '    ');
	lines.append('    // stop if buffer is full')
	lines.append('    if (++contour_length >= max_contour_length)' + (' // contour_length is the unsuppressed length' if dir == 0 and clockwise else ''))
	lines.append('        break;')
	lines.append('    // if border is to be suppressed, set pixel valid if {} is not border'.format(left))
	lines.append('    if (do_suppress_border)')
	lines.append('        is_pixel_valid = {};'.format(is_left_not_border_code(dir, clockwise)))
	lines.append('}')
	lines.append('// else (rule 3)')
	lines.append('else')
	lines.append('{')
	lines.append('    // turn {}'.format(right))
	new_dir = turn_right(dir, clockwise)
	lines.append('    dir = {};'.format(new_dir))
	#lines.append('    ++sum_of_turns;')
	if (sorted([dir, new_dir]) == [0, 3]):
		lines.append('    {}sum_of_turn_overflows;'.format('++' if (new_dir < dir) == clockwise else '--'))
	lines.append('    // set pixel valid if {} is not border'.format(left))
	lines.append('    if (!is_pixel_valid)')
	lines.append('        is_pixel_valid = {};'.format(is_left_not_border_code(new_dir, clockwise)))
	lines.append('}')
	return '\n'.join(indent + l for l in lines)


def make_rules_code_summary(dir, clockwise, indent):
	lines = [l for l in make_rules_code(dir, clockwise, indent).splitlines() if re.search(r"^\s*//", l)]
	return '\n'.join(l.replace("// ", "", 1) for l in lines)


def make_trace_step_code(dir, clockwise, indent):
	code = indent + "/*\n"
	code += make_rules_ascii_art(dir, clockwise, indent) + "\n"
	if dir == 0:
		code += "\n"
		code += make_rules_code_summary(dir, clockwise, indent) + "\n"
	code += indent + "*/\n\n"
	code += make_rules_code(dir, clockwise, indent)
	return code


def make_trace_generic_comment_code(indent):
	code = indent + "/*\n"
	code += make_rules_ascii_art(0, True, indent) + "\n"
	code += "\n"
	code += make_rules_code_summary(0, True, indent) + "\n"
	code += "\n"
	code += indent + "In case of counterclockwise tracing the rules are the same except that left and right are exchanged.\n"
	code += indent + "*/"
	return code


if False:  # test code
	for clockwise in (True, False):
		for dir in range(4):
			indent = ' ' * 2
			print('vvvvvvvvvvvvv')
			print(make_trace_step_code(dir, clockwise, indent))
			print('^^^^^^^^^^^^^')
	exit()


############################################################################################
########################################### main ###########################################
############################################################################################

with open(template_file) as f:
	template = f.read()

template = re.sub(r"\bo__NAMESPACE__o\b", namespace, template)
template = re.sub(r"\bo__NAMESPACE__o_\B", namespace_, template)  # FECTS_assert etc.
template = re.sub(r"\bo__WARNING_CODE_IS_GENERATED__o\b", lambda m: warning_generated_code, template)
template = re.sub(r"\bo__INTRODUCTION__o\b", lambda m: introduction, template)
lines = template.splitlines()

Preprocessor.Process(lines, ppvars, lambda t: "//o__#__o//" in t, template_file)

# remove lines which only comment the template or which contain code to pacify syntax checkers
lines = [l for l in lines if "//o__#__o//" not in l]

for line_index, line in enumerate(lines):
	m = re.match(r'^(\s*)(.*)\bo__TRACE_STEP_(C?CW)_DIR_([0-3])__o\b\s*;\s*(.*)$', line)
	if m:
		indent, code_before, clockwise, dir, code_after = m.groups()
		assert not code_before
		assert not code_after
		lines[line_index] = make_trace_step_code(int(dir), clockwise == 'CW', indent)
		continue

	m = re.match(r'^(\s*)(.*)\bo__TRACE_GENERIC_COMMENT__o\b\s*;\s*(.*)$', line)
	if m:
		indent, code_before, code_after = m.groups()
		assert not code_before
		assert not code_after
		lines[line_index] = make_trace_generic_comment_code(indent)
		continue

	while True:
		m = re.match(r'^(.*)\bo__isValueForeground\(\s*(.*?)\s*\)__o\b(.*)$', line)
		if m:
			code_before, args, code_after = m.groups()
			line = code_before + is_value_foreground_code(args) + code_after
			continue

		m = re.match(r'^(.*)\bo__isForwardBorderDir([0-3])__o\b(.*)$', line)
		if m:
			code_before, dir, code_after = m.groups()
			line = code_before + is_forward_border_no_m1_code(int(dir)) + code_after
			continue

		m = re.match(r'^(.*)\bo__TRACE_CONTINUE_CONDITION__o\b(.*)$', line)
		if m:
			code_before, code_after = m.groups()
			indent = re.sub(r'[^\s]', ' ', code_before)
			line = (code_before + "(x != start_x || y != start_y || dir != start_dir)\n" +
				indent + "&& (!is_stop_in || x != stop_x || y != stop_y || dir != stop_dir)" + code_after)
			continue

		lines[line_index] = line
		break

new_file_content = '\n'.join(lines) + '\n'

old_file_content = None
if os.path.isfile(output_file):
	with open(output_file) as f:
		old_file_content = f.read()

if new_file_content == old_file_content:
	print("No changes.")
else:
	if os.path.isfile(output_file):
		os.chmod(output_file, S_IREAD|S_IWUSR)  # make writable

	with open(output_file, 'w') as f:
		f.write(new_file_content)

	os.chmod(output_file, S_IREAD)  # make readonly
