# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""Artifact module.

This module provides the :class:`Artifact` class, which represents a file artifact that can be or is
stored on a remote node or locally.

Example usage of the :class:`Artifact` class:

    .. code:: python

        from framework.testbed_model.artifact import Artifact

        # Create an artifact on a remote node
        artifact = Artifact(node="sut", file_name="example.txt")
        # Open the artifact file in write mode
        with artifact.open("w") as f:
            f.write("Hello, World!")
        # Pull the artifact to the local output directory
        artifact.save_locally() # This is also done automatically on object deletion
                                # if save_local_copy is set to True
        # Check if the artifact exists
        if artifact.exists():
            print(f"Artifact exists at {artifact.path}")
        # Delete the artifact
        artifact.delete()

        # Create an artifact from a local file
        local_artifact = Artifact.create_from(
            original_file="local_file.txt",
            node="sut",
            file_name="copied_file.txt",
        )
        # Copy the content of the local artifact to another artifact
        another_artifact = Artifact("sut", "another_file.txt")
        another_artifact.copy_contents_from(local_artifact)
"""

import shutil
import uuid
from collections.abc import Iterable
from io import SEEK_SET, RawIOBase, TextIOWrapper
from pathlib import Path, PurePath
from typing import BinaryIO, ClassVar, Literal, TypeAlias, Union, cast, overload

from paramiko import SFTPClient, SFTPFile
from typing_extensions import Buffer

from framework.exception import InternalError
from framework.logger import DTSLogger, get_dts_logger
from framework.settings import SETTINGS
from framework.testbed_model.node import Node, NodeIdentifier, get_node

TextMode: TypeAlias = (
    Literal["r", "r+", "w", "w+", "a", "a+", "x", "x+"]
    | Literal["rt", "r+t", "wt", "w+t", "at", "a+t", "xt", "x+t"]
)
"""Open text mode for artifacts."""
BinaryMode: TypeAlias = Literal["rb", "r+b", "wb", "w+b", "ab", "a+b", "xb", "x+b"]
"""Open binary mode for artifacts."""
OpenMode: TypeAlias = TextMode | BinaryMode
"""Open mode for artifacts, can be either text or binary mode."""


@overload
def make_file_path(node: Node, file_name: str, custom_path: PurePath | None = None) -> PurePath: ...


@overload
def make_file_path(node: None, file_name: str, custom_path: PurePath | None = None) -> Path: ...


def make_file_path(
    node: Node | None, file_name: str, custom_path: PurePath | None = None
) -> Path | PurePath:
    """Make a file path for the artifact."""
    if node is None:
        path: Path | PurePath = Path(SETTINGS.output_dir).resolve()
    else:
        path = node.tmp_dir

    if custom_path is not None:
        if custom_path.is_absolute():
            return custom_path / file_name

        path /= custom_path
    else:
        from framework.context import get_ctx

        try:
            ctx = get_ctx()
            if ctx.local.current_test_suite is not None:
                path /= ctx.local.current_test_suite.name
            if ctx.local.current_test_case is not None:
                path /= ctx.local.current_test_case.name
        except InternalError:
            # If the context is not available, use the root path.
            pass

    return path / file_name


def make_unique_file_name() -> str:
    """Generate a unique filename for the artifact."""
    return f"{uuid.uuid4().hex}.dat"


class Artifact:
    """Artifact class.

    Represents a file artifact that can be or is stored on a remote node or locally. It provides
    methods to open, read, write, and manage the artifact file, in the same familiar Python API. It
    also provides functionality to save a local copy of the artifact if it is remote – saved in the
    test run output directory. It can be used to manage files that are part of the test run, such as
    logs, reports, or any other files that need to be stored for later analysis.

    By default, the artifact is created in the temporary directory of the node, following the tree
    directory structure defined by :class:`DirectoryTree` and managed by the test run states.

    The artifact file is not created automatically upon instantiation. The methods :meth:`open` –
    with either `w` or `a` modes – and :meth:`touch` can be used to create it.

    If `save_local_copy` is :data:`True` and there already exist a local file with the same name, it
    will be overwritten. If this is undesired, make sure to give distinct names to the artifacts.

    Attributes:
        save_local_copy: If :data:`True`, a local copy of the artifact will be saved at the end of
            the lifetime of the object automatically.
    """

    DIRECTORY_PERMISSIONS: ClassVar[int] = 0o755
    """Permission mode for directories created by the artifact."""
    TEXT_MODE_ENCODING: ClassVar[str] = "utf-8"
    """Encoding used for text mode artifacts."""
    TEXT_MODE_NEWLINE: ClassVar[str] = "\n"
    """Newline character used for text mode artifacts."""

    _logger: DTSLogger
    _node: Node | None = None
    _sftp: Union[SFTPClient, None] = None
    _fd: Union["ArtifactFile", None] = None
    _file_path: PurePath
    _local_path: Path
    _file_was_saved: bool = False
    _directories_created: bool = False
    save_local_copy: bool

    def __init__(
        self,
        node: NodeIdentifier,
        file_name: str = "",
        save_local_copy: bool = True,
        custom_path: PurePath | None = None,
    ):
        """Constructor for an artifact.

        Args:
            node: The node identifier on which the file is.
            file_name: The name of the file. If not provided, a unique filename will be generated.
            save_local_copy: If :data:`True`, makes a local copy of the artifact in the output
                directory. Applies only to remote artifacts.
            custom_path: A custom path to save the artifact. If :data:`None`, the default path will
                be used based on the node identifier. If a relative path is provided, it will be
                relative to the remote temporary directory (for remote artifacts) and local output
                directory (for local artifacts and copies).
        """
        self._logger = get_dts_logger(f"{node}_artifact")

        self._node = get_node(node)
        self.save_local_copy = save_local_copy

        if not file_name:
            file_name = make_unique_file_name()

        if self._node is not None:
            self._sftp = self._node.main_session.remote_session.session.sftp()

            if custom_path is not None and not custom_path.is_absolute():
                relative_custom_path = custom_path
            else:
                relative_custom_path = None

            self._file_path = make_file_path(self._node, file_name, custom_path)
            self._local_path = make_file_path(
                node=None, file_name=file_name, custom_path=relative_custom_path
            )
        else:
            self._local_path = self._file_path = make_file_path(self._node, file_name, custom_path)

    @overload
    def open(
        self,
        file_mode: BinaryMode = "rb",
        buffering: int = -1,
    ) -> "ArtifactFile": ...

    @overload
    def open(
        self,
        file_mode: TextMode = "r",
        buffering: int = -1,
    ) -> TextIOWrapper: ...

    def open(
        self, file_mode: BinaryMode | TextMode = "rb", buffering: int = -1
    ) -> Union["ArtifactFile", TextIOWrapper]:
        """Open the artifact file.

        Args:
            file_mode: The mode of file opening.
            buffering: The size of the buffer to use. If -1, the default buffer size is used.

        Returns:
            An instance of :class:`ArtifactFile` or :class:`TextIOWrapper`.
        """
        if self._fd is not None and not self._fd.closed:
            self._logger.warning(
                f"Artifact {self.path} is already open. Closing the previous file descriptor."
            )
            self._fd.close()
        elif not self._directories_created:
            self.mkdir()

        # SFTPFile does not support text mode, therefore everything needs to be handled as binary.
        if "t" in file_mode:
            actual_mode = cast(BinaryMode, cast(str, file_mode).replace("t", "") + "b")
        elif "b" not in file_mode:
            actual_mode = cast(BinaryMode, file_mode + "b")
        else:
            actual_mode = cast(BinaryMode, file_mode)

        self._logger.debug(f"Opening file at {self.path} with mode {file_mode}.")
        if self._sftp is None:
            fd: BinaryIO | SFTPFile = open(str(self.path), mode=actual_mode, buffering=buffering)
        else:
            fd = self._sftp.open(str(self.path), mode=actual_mode, bufsize=buffering)

        self._fd = ArtifactFile(fd, self.path, file_mode)

        if "b" in file_mode:
            return self._fd
        else:
            return TextIOWrapper(
                self._fd,
                encoding=self.TEXT_MODE_ENCODING,
                newline=self.TEXT_MODE_NEWLINE,
                write_through=True,
            )

    @classmethod
    def create_from(
        cls,
        original_file: Union[Path, "Artifact"],
        node: NodeIdentifier,
        /,
        new_file_name: str = "",
        save_local_copy: bool = False,
        custom_path: PurePath | None = None,
    ) -> "Artifact":
        """Create a new artifact from a local file or another artifact.

        Args:
            node: The node identifier on which the file is.
            original_file: The local file or artifact to copy.
            new_file_name: The name of the new file. If not provided, the name of the original file
                will be used.
            save_local_copy: Makes a local copy of the artifact if :data:`True`. Applies only to
                remote files.
            custom_path: A custom path to save the artifact. If :data:`None`, the default path will
                be used based on the node identifier.

        Returns:
            An instance of :class:`Artifact`.
        """
        if not new_file_name:
            if isinstance(original_file, Artifact):
                new_file_name = original_file.local_path.name
            else:
                new_file_name = original_file.name

        artifact = cls(node, new_file_name, save_local_copy, custom_path)
        artifact.copy_contents_from(original_file)
        return artifact

    def copy_contents_from(self, original_file: Union[Path, "Artifact"]) -> None:
        """Copy the content of another file or artifact into this artifact.

        This action will close the file descriptor associated with `self` or `original_file` if
        open.

        Args:
            original_file: The local file or artifact to copy.

        Raises:
            InternalError: If the provided `original_file` does not exist.
        """
        if isinstance(original_file, Path) and not original_file.exists():
            raise InternalError(f"The provided file '{original_file}' does not exist.")

        self.open("wb").close()  # Close any prior fd and truncate file.

        self._logger.debug(f"Copying content from {original_file} to {self}.")
        match (original_file, self._sftp):
            case (Path(), None):  # local file to local artifact
                # Use syscalls to copy
                shutil.copyfile(original_file, self.path)
            case (Artifact(_sftp=None), None):  # local artifact to local artifact
                # Use syscalls to copy
                shutil.copyfile(original_file.local_path, self.path)
            case (Artifact(), None):  # remote artifact to local artifact
                # Use built-in chunked transfer copy
                with original_file.open("rb") as original_fd:
                    with self.open("wb") as copy_fd:
                        shutil.copyfileobj(original_fd, copy_fd)
            case (_, SFTPClient()):  # remote artifact to remote artifact
                # Use SFTPClient's buffered file copy
                with original_file.open("rb") as original_fd:
                    self._sftp.putfo(original_fd, str(self.path))

    @property
    def path(self) -> PurePath:
        """Return the actual path of the artifact."""
        return self._file_path

    @property
    def local_path(self) -> Path:
        """Return the local path of the artifact."""
        return self._local_path

    def save_locally(self) -> None:
        """Copy remote artifact file and save it locally. Does nothing on local artifacts.

        If there already exist a local file with the same name, it will be overwritten. If this is
        undesired, make sure to give distinct names to the artifacts.
        """
        if self._sftp is not None:
            if not self.exists():
                self._logger.debug(f"File {self.path} was never created, skipping save.")
                return

            self._logger.debug(f"Pulling artifact {self.path} to {self.local_path}.")

            if not self._file_was_saved and self.local_path.exists():
                self._logger.warning(
                    f"While saving a remote artifact: local file {self.local_path} already exists, "
                    "overwriting it. Please use distinct file names."
                )
                self._file_was_saved = True

            self._sftp.get(str(self.path), str(self.local_path))

    def delete(self, remove_local_copy: bool = True) -> None:
        """Delete the artifact file. It also prevents a local copy from being saved.

        Args:
            remove_local_copy: If :data:`True`, the local copy of the artifact will be deleted if
                it already exists.
        """
        self._logger.debug(f"Deleting artifact {self.path}.")

        if self._fd is not None and not self._fd.closed:
            self._fd.close()
            self._fd = None

        if self._sftp is not None:
            self._sftp.remove(str(self._file_path))

        if self._sftp is None or remove_local_copy:
            self.local_path.unlink(missing_ok=True)

    def touch(self, mode: int = 0o644) -> None:
        """Touch the artifact file, creating it if it does not exist.

        Args:
            mode: The permission mode to set for the artifact file, if just created.
        """
        if not self._directories_created:
            self.mkdir()

        self._logger.debug(f"Touching artifact {self.path} with mode {oct(mode)}.")
        if self._sftp is not None:
            file_path = str(self._file_path)
            try:
                self._sftp.stat(file_path)
            except FileNotFoundError:
                self._sftp.open(file_path, "w").close()
                self._sftp.chmod(file_path, mode)
        else:
            Path(self._file_path).touch(mode=mode)

    def chmod(self, mode: int = 0o644) -> None:
        """Change the permissions of the artifact file.

        Args:
            mode: The permission mode to set for the artifact file.
        """
        self._logger.debug(f"Changing permissions of {self.path} to {oct(mode)}.")
        if self._sftp is not None:
            self._sftp.chmod(str(self._file_path), mode)
        else:
            Path(self._file_path).chmod(mode)

    def exists(self) -> bool:
        """Check if the artifact file exists.

        Returns:
            :data:`True` if the artifact file exists, :data:`False` otherwise.
        """
        if self._sftp is not None:
            try:
                self._sftp.stat(str(self._file_path))
                return True
            except FileNotFoundError:
                return False
        else:
            return self._local_path.exists()

    def mkdir(self) -> None:
        """Create all the intermediate file path directories."""
        if self._sftp is not None:
            parts = self._file_path.parts[:-1]
            paths = (PurePath(*parts[:tree_depth]) for tree_depth in range(1, len(parts) + 1))
            for path in paths:
                try:
                    self._sftp.stat(str(path))
                except FileNotFoundError:
                    self._logger.debug(f"Creating directories {path}.")
                    self._sftp.mkdir(str(path), mode=self.DIRECTORY_PERMISSIONS)

        if self._sftp is None or self.save_local_copy:
            self._logger.debug(f"Creating directories {self.local_path.parent} locally.")
            self.local_path.parent.mkdir(
                mode=self.DIRECTORY_PERMISSIONS, parents=True, exist_ok=True
            )

        self._directories_created = True

    def __del__(self):
        """Close the file descriptor if it is open and save it if requested."""
        if self._fd is not None and not self._fd.closed:
            self._fd.close()
            self._fd = None

        if self.save_local_copy:
            self.save_locally()

    def __str__(self):
        """Return path of the artifact."""
        return str(self.path)


class ArtifactFile(RawIOBase, BinaryIO):
    """Artifact file wrapper class.

    Provides a single interface for either local or remote files.
    This class implements the :class:`~io.RawIOBase` interface, allowing it to be used
    interchangeably with standard file objects.
    """

    _fd: Union[BinaryIO, SFTPFile]
    _path: PurePath
    _mode: OpenMode

    def __init__(self, fd: Union[BinaryIO, SFTPFile], path: PurePath, mode: OpenMode):
        """Initialize the artifact file wrapper.

        Args:
            fd: The file descriptor of the artifact.
            path: The path of the artifact file.
            mode: The mode in which the artifact file was opened.
        """
        super().__init__()
        self._fd = fd
        self._path = path
        self._mode = mode

    def close(self) -> None:
        """Close artifact file.

        This method implements :meth:`~io.RawIOBase.close()`.
        """
        self._fd.close()

    def read(self, size: int | None = -1) -> bytes:
        """Read bytes from the artifact file.

        This method implements :meth:`~io.RawIOBase.read()`.
        """
        return self._fd.read(size if size is not None else -1)

    def readline(self, size: int | None = -1) -> bytes:
        """Read line from the artifact file.

        This method implements :meth:`~io.RawIOBase.readline()`.
        """
        if size is None:
            size = -1  # Turning None to -1 due to abstract type mismatch.
        return self._fd.readline(size)

    def readlines(self, hint: int = -1) -> list[bytes]:
        """Read lines from the artifact file.

        This method implements :meth:`~io.RawIOBase.readlines()`.
        """
        return self._fd.readlines(hint)

    def write(self, data: Buffer) -> int:
        """Write bytes to the artifact file.

        Returns the number of bytes written if available, otherwise -1.

        This method implements :meth:`~io.RawIOBase.write()`.
        """
        return self._fd.write(data) or -1

    def writelines(self, lines: Iterable[Buffer]):
        """Write lines to the artifact file.

        This method implements :meth:`~io.RawIOBase.writelines()`.
        """
        return self._fd.writelines(lines)

    def flush(self) -> None:
        """Flush the write buffers to the artifact file if applicable.

        This method implements :meth:`~io.RawIOBase.flush()`.
        """
        self._fd.flush()

    def seek(self, offset: int, whence: int = SEEK_SET) -> int:
        """Change the file position to the given byte offset.

        This method implements :meth:`~io.RawIOBase.seek()`.
        """
        pos = self._fd.seek(offset, whence)
        if pos is None:
            return self._fd.tell()
        return pos

    def tell(self) -> int:
        """Return the current absolute file position.

        This method implements :meth:`~io.RawIOBase.tell()`.
        """
        return self._fd.tell()

    def truncate(self, size: int | None = None) -> int:
        """Change the size of the file to `size` or to the current position.

        This method implements :meth:`~io.RawIOBase.truncate()`.
        """
        if size is None:
            size = self._fd.tell()
        new_size = self._fd.truncate(size)
        if new_size is None:
            return size
        return new_size

    @property
    def name(self) -> str:
        """Return the name of the artifact file.

        This method implements :meth:`~io.RawIOBase.name()`.
        """
        return str(self._path)

    @property
    def mode(self) -> str:
        """Return the mode in which the artifact file was opened.

        This method implements :meth:`~io.RawIOBase.mode()`.
        """
        return self._mode

    @property
    def closed(self) -> bool:
        """:data:`True` if the file is closed."""
        return self._fd.closed

    def fileno(self) -> int:
        """Return the underlying file descriptor.

        This method implements :meth:`~io.RawIOBase.fileno()`.
        """
        return self._fd.fileno() if hasattr(self._fd, "fileno") else -1

    def isatty(self) -> bool:
        """Return :data:`True` if the file is connected to a terminal device.

        This method implements :meth:`~io.RawIOBase.isatty()`.
        """
        return self._fd.isatty() if hasattr(self._fd, "isatty") else False

    def readable(self) -> bool:
        """Return :data:`True` if the file is readable.

        This method implements :meth:`~io.RawIOBase.readable()`.
        """
        return self._fd.readable()

    def writable(self) -> bool:
        """Return :data:`True` if the file is writable.

        This method implements :meth:`~io.RawIOBase.writable()`.
        """
        return self._fd.writable()

    def seekable(self) -> bool:
        """Return :data:`True` if the file is seekable.

        This method implements :meth:`~io.RawIOBase.seekable()`.
        """
        return self._fd.seekable()

    def __enter__(self) -> "ArtifactFile":
        """Enter the runtime context related to this object.

        This method implements :meth:`~io.RawIOBase.__enter__()`.
        """
        return self

    def __exit__(self, *args) -> None:
        """Exit the runtime context related to this object.

        This method implements :meth:`~io.RawIOBase.__exit__()`.
        """
        self.close()
