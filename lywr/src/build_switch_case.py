with open("opcodes.txt") as f:
	for l in f:
		l = l.strip();
		if len(l) > 0 and l[0] == 'E':
			opcode_label = l.split(",")[2];
			print("case lywr_%s: {"%opcode_label);
			print("\t}break;");
