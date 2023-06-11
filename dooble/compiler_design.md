# Compiler Design

### Basic Compile Structure
```
compile():
    s := init_semantics()
    for files:
        ast := get_ast(file, s.tree)
        add_semantic_info(s, ast)

    semantic_pass0(s)
    semantic_pass1(s)
    semantic_pass2(s)

    generate_out(s)
```
