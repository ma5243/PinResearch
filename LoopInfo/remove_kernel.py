filepath="Instructions.txt"
with open(filepath) as f:
    printLoop = True
    currentLoop = ""
    for line in f:
        if line == "end\n":
            if printLoop:
                print(currentLoop, end='')
                print("end")
            printLoop = True
            currentLoop = ""
        else: 
            if int(line) > 0x7fffffff:
                printLoop = False

            currentLoop += line
            
