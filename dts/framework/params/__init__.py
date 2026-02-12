# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Parameter manipulation module.

This module provides :class:`Params` which can be used to model any data structure
that is meant to represent any command line parameters.
"""

from dataclasses import dataclass, fields
from enum import Flag
from typing import (
    Any,
    Callable,
    Iterable,
    Literal,
    Reversible,
    TypedDict,
    TypeVar,
    cast,
)

from typing_extensions import Self

T = TypeVar("T")

#: Type for a function taking one argument.
FnPtr = Callable[[T], T]
#: Type for a switch parameter.
Switch = Literal[True, None]
#: Type for a yes/no switch parameter.
YesNoSwitch = Literal[True, False, None]


def _reduce_functions(funcs: Iterable[FnPtr]) -> FnPtr:
    """Reduces an iterable of :attr:`FnPtr` from left to right to a single function.

    If the iterable is empty, the created function just returns its fed value back.

    Args:
        funcs: An iterable containing the functions to be chained from left to right.

    Returns:
        FnPtr: A function that calls the given functions from left to right.
    """

    def reduced_fn(value: T) -> T:
        for fn in funcs:
            value = fn(value)
        return value

    return reduced_fn


def modify_str(*funcs: FnPtr) -> Callable[[T], T]:
    r"""Class decorator modifying the ``__str__`` method with a function created from its arguments.

    The :attr:`FnPtr`\s fed to the decorator are executed from left to right in the arguments list
    order.

    Args:
        *funcs: The functions to chain from left to right.

    Returns:
        The decorator.

    Example:
        .. code:: python

            @convert_str(hex_from_flag_value)
            class BitMask(enum.Flag):
                A = auto()
                B = auto()

        will allow ``BitMask`` to render as a hexadecimal value.
    """

    def _class_decorator(original_class: T) -> T:
        setattr(original_class, "__str__", _reduce_functions(funcs))
        return original_class

    return _class_decorator


def comma_separated(values: Iterable[Any]) -> str:
    """Converts an iterable into a comma-separated string.

    Args:
        values: An iterable of objects.

    Returns:
        A comma-separated list of stringified values.
    """
    return ",".join([str(value).strip() for value in values if value is not None])


def bracketed(value: str) -> str:
    """Adds round brackets to the input.

    Args:
        value: Any string.

    Returns:
        A string surrounded by round brackets.
    """
    return f"({value})"


def str_from_flag_value(flag: Flag) -> str:
    """Returns the value from a :class:`enum.Flag` as a string.

    Args:
        flag: An instance of :class:`Flag`.

    Returns:
        The stringified value of the given flag.
    """
    return str(flag.value)


def hex_from_flag_value(flag: Flag) -> str:
    """Returns the value from a :class:`enum.Flag` converted to hexadecimal.

    Args:
        flag: An instance of :class:`Flag`.

    Returns:
        The value of the given flag in hexadecimal representation.
    """
    return hex(flag.value)


class ParamsModifier(TypedDict, total=False):
    """Params modifiers dict compatible with the :func:`dataclasses.field` metadata parameter."""

    #:
    Params_short: str
    #:
    Params_long: str
    #:
    Params_multiple: bool
    #:
    Params_convert_value: Reversible[FnPtr]


@dataclass
class Params:
    """Dataclass that renders its fields into command line arguments.

    The parameter name is taken from the field name by default. The following:

    .. code:: python

        name: str | None = "value"

    is rendered as ``--name=value``.

    Through :func:`dataclasses.field` the resulting parameter can be manipulated by applying
    this class' metadata modifier functions. These return regular dictionaries which can be combined
    together using the pipe (OR) operator, as used in the example for :meth:`~Params.multiple`.

    To use fields as switches, set the value to ``True`` to render them. If you
    use a yes/no switch you can also set ``False`` which would render a switch
    prefixed with ``--no-``. Examples:

    .. code:: python

        interactive: Switch = True  # renders --interactive
        numa: YesNoSwitch   = False # renders --no-numa

    Setting ``None`` will prevent it from being rendered. The :attr:`~Switch` type alias is provided
    for regular switches, whereas :attr:`~YesNoSwitch` is offered for yes/no ones.

    An instance of a dataclass inheriting ``Params`` can also be assigned to an attribute,
    this helps with grouping parameters together.
    The attribute holding the dataclass will be ignored and the latter will just be rendered as
    expected.
    """

    _suffix = ""
    """Holder of the plain text value of Params when called directly. A suffix for child classes."""

    """========= BEGIN FIELD METADATA MODIFIER FUNCTIONS ========"""

    @staticmethod
    def short(name: str) -> ParamsModifier:
        """Overrides any parameter name with the given short option.

        Args:
            name: The short parameter name.

        Returns:
            ParamsModifier: A dictionary for the `dataclasses.field` metadata argument containing
                the parameter short name modifier.

        Example:
            .. code:: python

                logical_cores: str | None = field(default="1-4", metadata=Params.short("l"))

            will render as ``-l=1-4`` instead of ``--logical-cores=1-4``.
        """
        return ParamsModifier(Params_short=name)

    @staticmethod
    def long(name: str) -> ParamsModifier:
        """Overrides the inferred parameter name to the specified one.

        Args:
            name: The long parameter name.

        Returns:
            ParamsModifier: A dictionary for the `dataclasses.field` metadata argument containing
                the parameter long name modifier.

        Example:
            .. code:: python

                x_name: str | None = field(default="y", metadata=Params.long("x"))

            will render as ``--x=y``, but the field is accessed and modified through ``x_name``.
        """
        return ParamsModifier(Params_long=name)

    @staticmethod
    def multiple() -> ParamsModifier:
        """Specifies that this parameter is set multiple times. The parameter type must be a list.

        Returns:
            ParamsModifier: A dictionary for the `dataclasses.field` metadata argument containing
                the multiple parameters modifier.

        Example:
            .. code:: python

                ports: list[int] | None = field(
                    default_factory=lambda: [0, 1, 2],
                    metadata=Params.multiple() | Params.long("port")
                )

            will render as ``--port=0 --port=1 --port=2``.
        """
        return ParamsModifier(Params_multiple=True)

    @staticmethod
    def convert_value(*funcs: FnPtr) -> ParamsModifier:
        """Takes in a variable number of functions to convert the value text representation.

        Functions can be chained together, executed from left to right in the arguments list order.

        Args:
            *funcs: The functions to chain from left to right.

        Returns:
            ParamsModifier: A dictionary for the `dataclasses.field` metadata argument containing
                the convert value modifier.

        Example:
            .. code:: python

                hex_bitmask: int | None = field(
                    default=0b1101,
                    metadata=Params.convert_value(hex) | Params.long("mask")
                )

            will render as ``--mask=0xd``.
        """
        return ParamsModifier(Params_convert_value=funcs)

    """========= END FIELD METADATA MODIFIER FUNCTIONS ========"""

    def append_str(self, text: str) -> None:
        """Appends a string at the end of the string representation.

        Args:
            text: Any text to append at the end of the parameters string representation.
        """
        self._suffix += text

    def __iadd__(self, text: str) -> Self:
        """Appends a string at the end of the string representation.

        Args:
            text: Any text to append at the end of the parameters string representation.

        Returns:
            The given instance back.
        """
        self.append_str(text)
        return self

    @classmethod
    def from_str(cls, text: str) -> Self:
        """Creates a plain Params object from a string.

        Args:
            text: The string parameters.

        Returns:
            A new plain instance of :class:`Params`.
        """
        obj = cls()
        obj.append_str(text)
        return obj

    @staticmethod
    def _make_switch(
        name: str, is_short: bool = False, is_no: bool = False, value: str | None = None
    ) -> str:
        """Make the string representation of the parameter.

        Args:
            name: The name of the parameters.
            is_short: If the parameters is short or not.
            is_no: If the parameter is negated or not.
            value: The value of the parameter.

        Returns:
            The complete command line parameter.
        """
        prefix = f"{'-' if is_short else '--'}{'no-' if is_no else ''}"
        name = name.replace("_", "-")
        value = f"{' ' if is_short else '='}{value}" if value else ""
        return f"{prefix}{name}{value}"

    def __str__(self) -> str:
        """Returns a string of command-line-ready arguments from the class fields."""
        arguments: list[str] = []

        for field in fields(self):
            value = getattr(self, field.name)
            modifiers = cast(ParamsModifier, field.metadata)

            if value is None:
                continue

            if isinstance(value, Params):
                arguments.append(str(value))
                continue

            # take the short modifier, or the long modifier, or infer from field name
            switch_name = modifiers.get("Params_short", modifiers.get("Params_long", field.name))
            is_short = "Params_short" in modifiers

            if isinstance(value, bool):
                arguments.append(self._make_switch(switch_name, is_short, is_no=(not value)))
                continue

            convert = _reduce_functions(modifiers.get("Params_convert_value", []))
            multiple = modifiers.get("Params_multiple", False)

            values = value if multiple else [value]
            for value in values:
                arguments.append(self._make_switch(switch_name, is_short, value=convert(value)))

        if self._suffix:
            arguments.append(self._suffix)

        return " ".join(arguments)
