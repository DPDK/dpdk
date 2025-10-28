# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Parsing utility module.

This module provides :class:`~TextParser` which can be used to model any dataclass to a block of
text.
"""

import re
from abc import ABC
from dataclasses import MISSING, dataclass, fields
from functools import partial
from typing import Any, Callable, TypedDict, cast

from typing_extensions import Self

from framework.exception import InternalError


class ParserFn(TypedDict):
    """Parser function in a dict compatible with the :func:`dataclasses.field` metadata param."""

    #:
    TextParser_fn: Callable[[str], Any]


@dataclass
class TextParser(ABC):
    r"""Helper abstract dataclass that parses a text according to the fields' rules.

    In order to enable text parsing in a dataclass, subclass it with :class:`TextParser`.

    The provided `parse` method is a factory which parses the supplied text and creates an instance
    with populated dataclass fields. This takes text as an argument and for each field in the
    dataclass, the field's parser function is run against the whole text. The returned value is then
    assigned to the field of the new instance. If the field does not have a parser function its
    default value or factory is used instead. If no default is available either, an exception is
    raised.

    This class provides a selection of parser functions and a function to wrap parser functions with
    generic functions. Parser functions are designed to be passed to the fields' metadata param. The
    most commonly used parser function is expected to be the `find` method, which runs a regular
    expression against the text to find matches.

    Example:
        The following example makes use of and demonstrates every parser function available:

        .. code:: python

            from dataclasses import dataclass, field
            from enum import Enum
            from framework.parser import TextParser

            class Colour(Enum):
                BLACK = 1
                WHITE = 2

                @classmethod
                def from_str(cls, text: str):
                    match text:
                        case "black":
                            return cls.BLACK
                        case "white":
                            return cls.WHITE
                        case _:
                            return None # unsupported colour

                @classmethod
                def make_parser(cls):
                    # make a parser function that finds a match and
                    # then makes it a Colour object through Colour.from_str
                    return TextParser.wrap(TextParser.find(r"is a (\w+)"), cls.from_str)

            @dataclass
            class Animal(TextParser):
                kind: str = field(metadata=TextParser.find(r"is a \w+ (\w+)"))
                name: str = field(metadata=TextParser.find(r"^(\w+)"))
                colour: Colour = field(metadata=Colour.make_parser())
                age: int = field(metadata=TextParser.find_int(r"aged (\d+)"))

            steph = Animal.parse("Stephanie is a white cat aged 10")
            print(steph) # Animal(kind='cat', name='Stephanie', colour=<Colour.WHITE: 2>, age=10)
    """

    """============ BEGIN PARSER FUNCTIONS ============"""

    @staticmethod
    def wrap(parser_fn: ParserFn, wrapper_fn: Callable) -> ParserFn:
        """Makes a wrapped parser function.

        `parser_fn` is called and if a non-None value is returned, `wrapper_function` is called with
        it. Otherwise the function returns early with None. In pseudo-code::

            intermediate_value := parser_fn(input)
            if intermediary_value is None then
                output := None
            else
                output := wrapper_fn(intermediate_value)

        Args:
            parser_fn: The dictionary storing the parser function to be wrapped.
            wrapper_fn: The function that wraps `parser_fn`.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing the
                newly wrapped parser function.
        """
        inner_fn = parser_fn["TextParser_fn"]

        def _composite_parser_fn(text: str) -> Any:
            intermediate_value = inner_fn(text)
            if intermediate_value is None:
                return None
            return wrapper_fn(intermediate_value)

        return ParserFn(TextParser_fn=_composite_parser_fn)

    @staticmethod
    def find_all(
        pattern: str | re.Pattern[str],
        flags: re.RegexFlag = re.RegexFlag(0),
    ) -> ParserFn:
        """Makes a parser function that finds all of the regular expression matches in the text.

        If there are no matches found in the text than None will be returned, otherwise a list
        containing all matches will be returned. Patterns that contain multiple groups will pack
        the matches for each group into a tuple.

        Args:
            pattern: The regular expression pattern.
            flags: The regular expression flags. Ignored if the given pattern is already compiled.

        Returns:
            A :class:`ParserFn` that can be used as metadata for a dataclass field.
        """
        if isinstance(pattern, str):
            pattern = re.compile(pattern, flags)

        def _find_all(text: str) -> list[str] | None:
            m = pattern.findall(text)
            if len(m) == 0:
                return None

            return m

        return ParserFn(TextParser_fn=_find_all)

    @staticmethod
    def find(
        pattern: str | re.Pattern[str],
        flags: re.RegexFlag = re.RegexFlag(0),
        named: bool = False,
    ) -> ParserFn:
        """Makes a parser function that finds a regular expression match in the text.

        If the pattern has any capturing groups, it returns None if no match was found, otherwise a
        tuple containing the values per each group is returned. If the pattern has only one
        capturing group and a match was found, its value is returned. If the pattern has no
        capturing groups then either True or False is returned if the pattern had a match or not.

        Args:
            pattern: The regular expression pattern.
            flags: The regular expression flags. Ignored if the given pattern is already compiled.
            named: If set to True only the named capturing groups will be returned, as a dictionary.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing the find
                parser function.
        """
        if isinstance(pattern, str):
            pattern = re.compile(pattern, flags)

        def _find(text: str) -> Any:
            m = pattern.search(text)
            if m is None:
                return None if pattern.groups > 0 else False

            if pattern.groups == 0:
                return True

            if named:
                return m.groupdict()

            matches = m.groups()
            if len(matches) == 1:
                return matches[0]

            return matches

        return ParserFn(TextParser_fn=_find)

    @staticmethod
    def find_int(
        pattern: str | re.Pattern[str],
        flags: re.RegexFlag = re.RegexFlag(0),
        int_base: int = 0,
    ) -> ParserFn:
        """Makes a parser function that converts the match of :meth:`~find` to int.

        This function is compatible only with a pattern containing one capturing group.

        Args:
            pattern: The regular expression pattern.
            flags: The regular expression flags. Ignored if the given pattern is already compiled.
            int_base: The base of the number to convert from.

        Raises:
            InternalError: If the pattern does not have exactly one capturing group.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing the
                :meth:`~find` parser function wrapped by the int built-in.
        """
        if isinstance(pattern, str):
            pattern = re.compile(pattern, flags)

        if pattern.groups != 1:
            raise InternalError("only one capturing group is allowed with this parser function")

        return TextParser.wrap(TextParser.find(pattern), partial(int, base=int_base))

    """============ END PARSER FUNCTIONS ============"""

    @classmethod
    def parse(cls, text: str) -> Self:
        """Creates a new instance of the class from the given text.

        A new class instance is created with all the fields that have a parser function in their
        metadata. Fields without one are ignored and are expected to have a default value, otherwise
        the class initialization will fail.

        A field is populated with the value returned by its corresponding parser function.

        Args:
            text: the text to parse

        Raises:
            InternalError: if the parser did not find a match and the field does not have a default
                value or default factory.

        Returns:
            A new instance of the class.
        """
        fields_values = {}
        for field in fields(cls):
            parse = cast(ParserFn, field.metadata).get("TextParser_fn")
            if parse is None:
                continue

            value = parse(text)
            if value is not None:
                fields_values[field.name] = value
            elif field.default is MISSING and field.default_factory is MISSING:
                raise InternalError(
                    f"parser for field {field.name} returned None, but the field has no default"
                )

        return cls(**fields_values)
