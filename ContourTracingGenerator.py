import os
import re

directory = os.path.dirname(os.path.abspath(__file__))
template_file = os.path.join(directory, "ContourTracingGeneratorTemplate.hpp")
output_file = os.path.join(directory, 'ContourTracing.hpp')

namespace = "FEPCT"

warning_generated_code = """
#############################################################################
# WARNING: this code was generated - do not edit, your changes may get lost #
#############################################################################
Consider to edit {} and {} instead.
""".strip('\n').format(os.path.basename(__file__), os.path.basename(template_file))

introduction = """
 Fast Edge-Based Pixel Contour Tracing from Seed Point
=======================================================


""".strip('\n')

"""
		   Definition of direction
		                                   x    
		  +---------------------------------->  
		  |               (0, -1)               
		  |                  0                  
		  |                  ^                  
		  |                  |                  
		  |                  |                  
		  | (-1, 0) 3 <------+------> 1 (1, 0)  
		  |                  |                  
		  |                  |                  
		  |                  v                  
		  |                  2                  
		y |               (0, 1)                
		  v                                     
"""
def vector(dir):
	return ((0, -1), (1, 0), (0, 1), (-1, 0))[dir]

def left_dir(dir, clockwise):
	return (3, 0, 1, 2)[dir] if clockwise else (1, 2, 3, 0)[dir]

def right_dir(dir, clockwise):
	return (1, 2, 3, 0)[dir] if clockwise else (3, 0, 1, 2)[dir]

def is_forward_border_code(dir):
	return ('y == 0', 'x == width_m1', 'y == height_m1', 'x == 0')[dir]

def is_not_forward_border_code(dir):
	return ('y != 0', 'x != width_m1', 'y != height_m1', 'x != 0')[dir]

def is_left_not_border_code(dir, clockwise):
	return is_not_forward_border_code(left_dir(dir, clockwise))

def forward_vector(dir, clockwise):
	return vector(dir)

def forward_left_vector(dir, clockwise):
	v1 = vector(dir)
	v2 = vector(left_dir(dir, clockwise))
	return (v1[0] + v2[0], v1[1] + v2[1])

def forward_pixel(dir, clockwise):
	return offset_pixel(forward_vector(dir, clockwise))

def forward_left_pixel(dir, clockwise):
	return offset_pixel(forward_left_vector(dir, clockwise))

def pixel_off_code(vec):
	sign = {-1: 'm', 0: '0', 1: 'p'}
	return "off_" + sign[vec[0]] + sign[vec[1]]

def is_pixel_foreground_code(vec):
	return "pixel[{}] != 0".format(pixel_off_code(vec))

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
	lines.append('    dir = {};'.format(right_dir(dir, clockwise)))
	lines.append('}')
	lines.append('// else if {0} is not border and forward-{0} pixel is foreground (rule 1)'.format(left))
	lines.append('else if ({} && {})'.format(is_left_not_border_code(dir, clockwise),
											 is_pixel_foreground_code(forward_left_vector(dir, clockwise))))
	lines.append('{')
	lines.append('    // emit current pixel')
	lines.append('    contour.emplace_back(x, y);')
	lines.append('    if (++contour_length >= max_contour_length)')
	lines.append('        break;')
	lines.append('    // go to checked pixel');
	lines += move_pixel_code_lines(forward_left_vector(dir, clockwise), '    ');
	lines.append('    // has_in_edge = true');
	lines.append('    has_in_edge = true;');
	lines.append('    // turn {}'.format(left));
	lines.append('    dir = {};'.format(left_dir(dir, clockwise)))
	lines.append('}')
	lines.append('// else if forward pixel is foreground (rule 2)')
	lines.append('else if ({})'.format(is_pixel_foreground_code(forward_vector(dir, clockwise))))
	lines.append('{')
	lines.append('    // if not do_suppress_border or has_in_edge')
	lines.append('    if (!do_suppress_border || has_in_edge)')
	lines.append('    {')
	lines.append('        // emit current pixel')
	lines.append('        contour.emplace_back(x, y);')
	lines.append('    }')
	lines.append('    if (++contour_length >= max_contour_length)' + (' // contour_length is the unsuppressed length' if dir == 0 and clockwise else ''))
	lines.append('        break;')
	lines.append('    // go to checked pixel');
	lines += move_pixel_code_lines(forward_vector(dir, clockwise), '    ');
	lines.append('    // has_in_edge = left is not border')
	lines.append('    has_in_edge = {};'.format(is_left_not_border_code(dir, clockwise)))
	lines.append('}')
	lines.append('// else (rule 3)')
	lines.append('else')
	lines.append('{')
	lines.append('    // turn {}'.format(right))
	lines.append('    dir = {};'.format(right_dir(dir, clockwise)))
	lines.append('    // has_in_edge = has_in_edge or {} is not border'.format(left))
	lines.append('    if (!has_in_edge)')
	lines.append('        has_in_edge = {};'.format(is_left_not_border_code(dir, clockwise)))
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
	code += make_rules_code(dir, clockwise, indent) + "\n"
	return code


if False:
	for clockwise in (True, False):
		for dir in range(4):
			indent = ' ' * 2
			print(make_trace_step_code(dir, clockwise, indent))
	exit()


with open(template_file) as f:
	template = f.read()

lines = (template.replace('o__NAMESPACE__o', namespace)
				 .replace('o__WARNING_CODE_IS_GENERATED__o', warning_generated_code)
				 .replace('o__INTRODUCTION__o', introduction)
				 .splitlines())

for line_index, line in enumerate(lines):
	m = re.match(r'^(\s*)(.*)o__TRACE_STEP_(C?CW)_DIR_([0-3])__o\s*;\s*(.*)$', line)
	if m:
		indent, code_before, clockwise, dir, code_after = m.groups()
		assert not code_before
		assert not code_after
		lines[line_index] = make_trace_step_code(int(dir), clockwise == 'CW', indent)

with open(output_file, 'w') as f:
	f.write('\n'.join(lines) + '\n')
