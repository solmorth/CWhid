Import("env")

def ignore_asm_files(node):
    if node.name.endswith(".asm"):
        return None
    return node

env.AddBuildMiddleware(ignore_asm_files, "*.asm")
