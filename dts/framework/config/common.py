# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Arm Limited

"""Common definitions and objects for the configuration."""

from collections.abc import Callable, MutableMapping
from typing import TYPE_CHECKING, Any, TypedDict, cast

from pydantic import BaseModel, ConfigDict, ValidationInfo

if TYPE_CHECKING:
    from framework.settings import Settings


class ValidationContext(TypedDict):
    """A context dictionary to use for validation."""

    #: The command line settings.
    settings: "Settings"


def load_fields_from_settings(
    *fields: str | tuple[str, str],
) -> Callable[[Any, ValidationInfo], Any]:
    """Before model validator that injects values from :attr:`ValidationContext.settings`.

    Args:
        *fields: The name of the fields to apply the argument value to. If the settings field name
            is not the same as the configuration field, supply a tuple with the respective names.

    Returns:
        Pydantic before model validator.
    """

    def _loader(data: Any, info: ValidationInfo) -> Any:
        if not isinstance(data, MutableMapping):
            return data

        settings = cast(ValidationContext, info.context)["settings"]
        for field in fields:
            if isinstance(field, tuple):
                settings_field = field[0]
                config_field = field[1]
            else:
                settings_field = config_field = field

            if settings_data := getattr(settings, settings_field):
                data[config_field] = settings_data

        return data

    return _loader


class FrozenModel(BaseModel):
    """A pre-configured :class:`~pydantic.BaseModel`."""

    #: Fields are set as read-only and any extra fields are forbidden.
    model_config = ConfigDict(frozen=True, extra="forbid")
