filepath="Instructions_no_kernel.txt"
with open(filepath) as f:
    isTail = True
    for line in f:
        line = line.strip()
        if (line) == "end":
            print(line)
        else:
            if isTail:
                print(line,end=' ')
            else:
                print(line)

            isTail = not isTail
