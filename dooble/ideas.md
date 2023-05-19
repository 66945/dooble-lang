# Dooble Ideas

## Syntax Ideas

### FizzBuzz
Using the concepts I come up with I am going to write an overcomplicated FizzBuzz program

### Consistant declarations
A declaration is any operation that binds something to a name.
Most languages have inconsistant ideas about declarations in different areas. They tend
to have a seperate declaration syntax for variables, functions, and sometimes types.

Examples of variable declarations
- go:         `name := val`
- C:          `type name = val`
- JavaScript: `let name = val`

Examples of function declarations
- go:        `func name(...) type {`
- C:         `type name(...) {`
- JavaScript `function name(...) {`

Examples of type declarations
- go:  `type alias original`
- C:   `typedef original alias`
- odin `alias :: original`

Odin, Jai, and Herb Sutters cppfront have consistant declarations across all variations.
```odin
const :: 0
func :: proc() {
}
Type :: struct {
}
```

For Dooble I like the way Odin and Jai do it: name -> type -> assignment -> value

### Functions
An anonymous function will take the form `() statement`.

```dooble
named :: () {
	use_anon(() {
		print('hello world')
	})

	lambda := () print('hi')
}
```

This would require me to have a token seperating the function and the type:
`name :: (): int {` vs `name :: () int {`
so that I could tell the difference between a lambda with and without a type.

Or I could just require braces, but that seems boring.

Other problems with braceless functions:
`multiple_lamdas(() return a;, () return b;)`

Don't do braceless functions for now

### Qualifier usage
A name first approach suggest that `static index := 0` should be invalid, but every
other place to put the qualifier is awkward. Using an attribute syntax such as
```dooble
@pub
emit_scope :: (cg: *CodeGen) {
	@static
	index := 4
}
```
just fills out more lines of code and doesn't add much readability. Adding them to the
type system means that you will have to write out the full type more often than not.

Placing the qualifiers before the value seems mostly okay, but it can lead to a few
interesting situations.
```dooble
emit_scope :: pub (cg: *CodeGen) {
	index := static 4
}
```

The question becomes: *How important is the name first syntax really?*
If the answer is *not very* then I can put the qualifiers first easy, but if the issue
is not one I am willing to compromise on then I might have to adopt a slightly uglier
syntax.

Another small problem is valueless variables:
```dooble
var := static 0
var: int -- cannot mark as static
```

I could mark things as `name` -> `qualifier` -> `type` -> `assignment`
```dooble
var pub :: 0
```

## Object Oriented Design
```dooble
Template :: struct {
	source:   *File
	mark:      string
	position:  u64
	output:    string
}

init_template :: (tp: *Template, name: string) errno {
	tp* = Template {
		source   = nil,
		mark     = '',
		position = 0,
		output   = '',
	}
}

method :: (tp: *Template) {
	position += 1
}

chained :: (tp: *Template) {
	...
}

main :: () int {
	tp := new init_template('hello world')

	tp.method()

	tp.method().
		chained().
		chained()

	return 0
}
```

### Unions
```dooble
Person :: sum {
	manager:  Manager
	employee: Employee
}

do_job :: (self: *Person.manager) {
	print('manager')
}

do_job :: (self: *Person.employee) {
	print('employee')
}

person := Person.manager(Manager {})
person.do_job() -- manager
person = .employee(Employee {})
person.do_job() -- employee
```

### `new` vs `make`
I have elected to use some *interesting* constructor syntax. Basically a function can be
turned into a constructor using ufcs.

If a variable assignment takes the form `var := new func()` then it will be equivalant
to:
```dooble
var := ReturnType {}
func(var)
```
There is the potential to expand this syntax to include heap allocations as well:
```dooble
var := alloc new func()
```

### Interfaces / Protocols
```dooble
Addable :: protocol<T> {
	add: (a: T) T
}

do_something :: (a: [$N]Addable, b: [N]Addable) {
	for i in 0 .. N {
		a[i].add(b[i])
	}
}
```

Protocols are similar to both go's interfaces and Rust's trait system. Like go they work
with the ufcs principles and do not require explicit extension. Like Rust, they are
expanded at compile time and do not use virtual functions. Rust allows for vtables with
the `impl trait` type, but I think that vtables should not exist unless explicitly created
in a struct like so:
```dooble
Addable :: struct<$T: type> {
	add: (a: T, b: T) T
}
```

Protocols could also be used for a rudimentary form of operator overloading. Mostly
useful for map types.

```
CustomPoolArr :: struct {
	
}
```

## Results and Options
`var: ? = nil` should be valid.
Unnamed errors and options are possible, which is mostly useful for function returns.
```dooble
release_tree :: (tree: *Tree) ! {

}
```
as opposed to
```dooble
release_tree :: (tree: *Tree) !nil {

}
```

In general I want to remove the idea of a *void* type as much as possible.

## Sum Types
sumtypes behave as tagged unions and as regular enums -> somewhat similar to rust enums.

```dooble
window :: sum {
	...
}
```

## Type inference
`var := someval` assumes the type of someval

For literals the inference is as follows:
```dooble
var := 0       -- int
var := 0.0     -- float
var := 0f      -- float
var := 'hello' -- str
var := 'h'     -- char

var := "C:\Users\Etc.\boom.png" -- string
```

## Generics
Unlike Odin, C++, and D, dooble's parametric polymorphism only applies to types.

```dooble
generic_add :: (a: $T, b T) T {
	return a + b
}

GenericData :: struct<T> {
	member: T
	test:   u32
}
```

## Reflection
### Compile attributes

```dooble
debug_print :: (ast: *AST) {
	if 
}

@debug_print
function :: () {
	...
}
```

## Entry points
Code can be written anywhere in a file and it will be executed in that order at runtime.
Global variables will still be turned into globals, but any statements will be wrapped
into a function. These functions will be called in the order that they are imported.

```dooble
-- in main.db
include 'hello.db'

Global :: 0
Mutable := 1

print('hello world')

-- in hello.db
print('hi')
```
becomes
```c
// main.c
const int Global = 0;
int Mutable = 1;

void in_main(void) {
	printf("hello world" "\n");
}

void in_hello(void) {
	printf("hi" "\n");
}

int main(int argc, char **argv) {
	in_main();
	in_hello();

	// ...
}
```

## Philosophy - A collection of random ideas justifying my poor decisions
