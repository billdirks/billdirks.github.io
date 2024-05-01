Title: Arbitrary Computation with Python Types
Date: 2024-04-30
Category: python
Tags: python, types
Slug: computation-with-python-types
Summary: Arbitrary computation in Python while never explicitly instantiating a class

# Introduction

I was reading about Python generics and dynamic types (related to my work and understanding some underlying Pydantic mechanics) and it made me wonder if I could do arbitrary computation with python types. That is, could I ergonomically do computations while never explicitly instantiating a class.

# Implementation

For this blog I'm going to look at classes that represent numbers and implement the ability to add these classes to produce a new class. To do this I'm going to leverage `__class_getitem__`. As background, in Python, one can implement `__getitem__` to implement what indexing into an object using `[]` does. For example:

**Note, this and the following examples were all executed in an ipython shell using python 3.8.17.**
```
class Indexable:
  def __getitem__(self, key):
    reutrn f'my_{key}'

indexable = Indexable()
indexable[1]  # 'my_1'
indexable["computer"]  # 'my_computer'
```

`__class_getitem__` is an analgous method for indexing classes and lets one implement using `[]` on a class. Usually (in all cases I've seen), the argument inside of `[]` is a class and is used for defining generics.

```
from typing import TypeVar, Generic

T = TypeVar('T')
class MyClass(Generic[T]): ...
```

Now one can instantiate classes via `MyClass[int]`, `MyClass[str]`, etc.

However, there is nothing restricting one to only pass classes to `[]`. One could pass arbitrary data. Combining this with the ability to dynamically make classes on the fly, one could generate a new class for a number. For example:

```
from types import new_class

class Number:
    number_cache = {}
    def __class_getitem__(cls, number: int):
        if number not in Number.number_cache:
            Number.number_cache[number] = new_class(
                f"Number{number}",
                bases=(),
                kwds=None,  # {"metaclass": MetaNumber},
                exec_body=lambda ns: ns.update(
                    {
                        "NUMBER": number,
                    }
                ),
            )
        return Number.number_cache[number]

# Create a number type
three = Number[3]
three  # <class 'types.Number3'>
type(three)  # type

# We can instantiate one if we want
three()  # <types.Number3 object at 0x1067fd700>
```

If we want to add we could define a function like:

```
def addNumbers(a, b):
  return Number[a.NUMBER + b.NUMBER]

addNumbers(Number[1],  Number[2]) == three  # True
```

We can now add classes to get new classes! We could make this more ergonomic by overloading the `+` sign to add these classes. Overloading `+` is done by implementing `__add__` on the class/type of an instance. However, the type of our dynamically generated types, say `Number3` is `type`, which is not modifiable. However, we can set a metaclass on the dynamically created type. Doing this will give us a hook to define `__add__` since the type of `Number3` will be our metaclass. Since `Number3` is a class, it is still a `type`.

```
from types import new_class

class MetaNumber(type):
    def __add__(cls, other):
        return Number[cls.NUMBER + other.NUMBER]

class Number:
    number_cache = {}
    def __class_getitem__(cls, number: int):
        if number not in Number.number_cache:
            Number.number_cache[number] = new_class(
                f"Number{number}",
                bases=(),
                kwds={"metaclass": MetaNumber},
                exec_body=lambda ns: ns.update(
                    {
                        "NUMBER": number,
                    }
                ),
            )
        return Number.number_cache[number]

# Adding Numbers
Number[3] + Number[4] == Number[7]  # True, adding works!
isinstance(Number[7], type)  # True, it's a type
type(Number[7]) # __main__.MetaNumber, MetaNumber is a type subclass
MetaNumber.mro(MetaNumber)  # [__main__.MetaNumber, type, object], the type object hierarchy.
```

# Conclusion

This broke my brain. By using `[]` to instantiate new types we can store arbitrary data on a type that is as ergonomic as instantiating a class with `()`. We can also put arbitary methods on the class as well as overload operators using metaclasses. This should give us the ability to do arbitrary computation using Python types.

Would one ever want to do this? No, this is tomfoolery. But, I couldn't resist trying to see if this was possible after reading some Pydantic code and the docs on `__class_getitem__`.
