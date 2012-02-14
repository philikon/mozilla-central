#!/usr/bin/python

import sys

if len (sys.argv) < 4:
	print >>sys.stderr, "usage: ./gen-indic-table.py IndicSyllabicCategory.txt IndicMatraCategory.txt Blocks.txt"
	sys.exit (1)

files = [file (sys.argv[i+1]) for i in range (3)]

headers = [[f.readline () for i in range (2)] for f in files]

blocks = {}
data = [{} for f in files]
values = [{} for f in files]
for i, f in enumerate (files):
	for line in f:

		j = line.find ('#')
		if j >= 0:
			line = line[:j]

		fields = [x.strip () for x in line.split (';')]
		if len (fields) == 1:
			continue

		uu = fields[0].split ('..')
		start = int (uu[0], 16)
		if len (uu) == 1:
			end = start
		else:
			end = int (uu[1], 16)

		t = fields[1]

		for u in range (start, end + 1):
			data[i][u] = t
		values[i][t] = values[i].get (t, 0) + 1

		if i == 2:
			blocks[t] = (start, end)

# Merge data into one dict:
defaults = ('Other', 'Not_Applicable', 'No_Block')
for i,v in enumerate (defaults):
	values[i][v] = values[i].get (v, 0) + 1
combined = {}
for i,d in enumerate (data):
	for u,v in d.items ():
		if i == 2 and not u in combined:
			continue
		if not u in combined:
			combined[u] = list (defaults)
		combined[u][i] = v
data = combined
del combined
num = len (data)

# Move the outliers NO-BREAK SPACE and DOTTED CIRCLE out
singles = {}
for u in [0x00A0, 0x25CC]:
	singles[u] = data[u]
	del data[u]

print "/* == Start of generated table == */"
print "/*"
print " * The following table is generated by running:"
print " *"
print " *   ./gen-indic-table.py IndicSyllabicCategory.txt IndicMatraCategory.txt Blocks.txt"
print " *"
print " * on files with these headers:"
print " *"
for h in headers:
	for l in h:
		print " * %s" % (l.strip())
print " */"

# Shorten values
print
short = [{
	"Bindu":		'Bi',
	"Visarga":		'Vs',
	"Vowel":		'Vo',
	"Vowel_Dependent":	'M',
	"Other":		'x',
},{
	"Not_Applicable":	'x',
}]
all_shorts = [[],[]]

# Add some of the values, to make them more readable, and to avoid duplicates


for i in range (2):
	for v,s in short[i].items ():
		all_shorts[i].append (s)

what = ["INDIC_SYLLABIC_CATEGORY", "INDIC_MATRA_CATEGORY"]
what_short = ["ISC", "IMC"]
for i in range (2):
	print
	vv = values[i].keys ()
	vv.sort ()
	for v in vv:
		v_no_and = v.replace ('_And_', '_')
		if v in short[i]:
			s = short[i][v]
		else:
			s = ''.join ([c for c in v_no_and if ord ('A') <= ord (c) <= ord ('Z')])
			if s in all_shorts[i]:
				raise Exception ("Duplicate short value alias", v, s)
			all_shorts[i].append (s)
			short[i][v] = s
		print "#define %s_%s	%s_%s	%s/* %3d chars; %s */" % \
			(what_short[i], s, what[i], v.upper (), \
			'	'* ((48-1 - len (what[i]) - 1 - len (v)) / 8), \
			values[i][v], v)
print
print "#define _(S,M) INDIC_COMBINE_CATEGORIES (ISC_##S, IMC_##M)"
print
print

def print_block (block, start, end, data):
	print
	print
	print "  /* %s  (%04X..%04X) */" % (block, start, end)
	num = 0
	for u in range (start, end+1):
		if u % 8 == 0:
			print
			print "  /* %04X */" % u,
		if u in data:
			num += 1
		d = data.get (u, defaults)
		sys.stdout.write ("%9s" % ("_(%s,%s)," % (short[0][d[0]], short[1][d[1]])))

	if num == 0:
		# Filler block, don't check occupancy
		return
	total = end - start + 1
	occupancy = num * 100. / total
	# Maintain at least 30% occupancy in the table */
	if occupancy < 30:
		raise Exception ("Table too sparse, please investigate: ", occupancy, block)

uu = data.keys ()
uu.sort ()

last = -1
num = 0
offset = 0
starts = []
ends = []
print "static const INDIC_TABLE_ELEMENT_TYPE indic_table[] = {"
for u in uu:
	if u <= last:
		continue
	block = data[u][2]
	(start, end) = blocks[block]

	if start != last + 1:
		if start - last <= 33:
			print_block ("FILLER", last+1, start-1, data)
			last = start-1
		else:
			if last >= 0:
				ends.append (last + 1)
				offset += ends[-1] - starts[-1]
			print
			print
			print "#define indic_offset_0x%04x %d" % (start, offset)
			starts.append (start)

	print_block (block, start, end, data)
	last = end
ends.append (last + 1)
offset += ends[-1] - starts[-1]
print
print
print "#define indic_offset_total %d" % offset
print
print "};"

print
print "static INDIC_TABLE_ELEMENT_TYPE"
print "get_indic_categories (hb_codepoint_t u)"
print "{"
for (start,end) in zip (starts, ends):
	offset = "indic_offset_0x%04x" % start
	print "  if (0x%04X <= u && u <= 0x%04X) return indic_table[u - 0x%04X + %s];" % (start, end, start, offset)
for u,d in singles.items ():
	print "  if (unlikely (u == 0x%04X)) return _(%s,%s);" % (u, short[0][d[0]], short[1][d[1]])
print "  return _(x,x);"
print "}"

print
print "#undef _"
for i in range (2):
	print
	vv = values[i].keys ()
	vv.sort ()
	for v in vv:
		print "#undef %s_%s" % \
			(what_short[i], short[i][v])

print
print
print "/* == End of generated table == */"
