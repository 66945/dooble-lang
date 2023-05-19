Code Generation API
===================

An api designed to build a C AST and generate code dynamically

## Basic design
The basic building blocks are `function`s, `scope`s, and `expression`s.
I would like for the interface to work somewhat like emitting bytecode, but
that might not quite work for this.

### `function`
*Represents a procedure*

Contains
- name
- parameter list
- return type

### `scope`
*Represents a block of code*

Contains
- list of identifiers

### `expression`
*A broad catagory of items*

### `identifier`
*Unique identifiers*

Contains
- a name
- a unique ID
- type information

### `atom`
*A basic compilation unit*

A sum type containing
- booleans
- strings
- numbers

## Emit style
I want to be able to 'emit' ast information in a sequential manner and have it be
reasonably intelligent about the information it recieves.

For example, the sequence
```
EMIT_NUMBER(3);
EMIT_NUMBER(4);
EMIT_ADD();
```
should translate to
`3 + 4`

This is made somewhat complicated by ast elements such as:
- loops
- if statements
- functions

### Scope principle
1. At the end of every loop, function, conditional, etc. I close the scope.
2. This will remain the same, regardless of what specific code has been emitted.

Therefore I can always call `EMIT_END()` to close the active scope

**Should I use a seperate node for scope end?**

### Stack based implementation
Every `emit` will intelligently react to whatever items already exist on the stack,
moving around prior elements as needed

For example, the above emits would produce the following behavior on the stack:
`EMIT_NUMBER(3)` -> [ atomic 3                            ]
`EMIT_NUMBER(4)` -> [ atomic 3, atomic 4                  ]
`EMIT_ADD()`     -> [ atomic 3, atomic 4, add ref 3 ref 4 ]

### Convinience macros
A lot of common sequences can be built into macros instead of manual emit statements.
`EMIT_ADD(3, 4)`, for example.

An `EMIT_C("{} + {}")` would also be nice, expecially if it had a decent template system.
Maybe `{}` could represent stack elements, with `{N}` counting off in reverse order.

This would actually eliminate the need for much of the AST elements allowing for a nicer
API.
