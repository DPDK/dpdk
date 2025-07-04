# DTS Environment

The execution and development environments for DTS are the same,
a [Docker](https://docs.docker.com/) container defined by our [Dockerfile](./Dockerfile).
Using a container for the development environment helps with a few things.

1. It helps enforce the boundary between the DTS environment and the TG/SUT,
   something which caused issues in the past.
2. It makes creating containers to run DTS inside automated tooling much easier, since
   they can be based off of a known-working environment that will be updated as DTS is.
3. It abstracts DTS from the server it is running on. This means that the bare-metal OS
   can be whatever corporate policy or your personal preferences dictate,
   and DTS does not have to try to support all distros that are supported by DPDK CI.
4. It makes automated testing for DTS easier,
   since new dependencies can be sent in with the patches.
5. It fixes the issue of undocumented dependencies,
   where some test suites require Python libraries that are not installed.
6. Allows everyone to use the same Python version easily,
   even if they are using a distribution or Windows with out-of-date packages.
7. Allows you to run the tester on Windows while developing via Docker for Windows.

## Tips for setting up a development environment

### Getting a docker shell

These commands will give you a bash shell inside the container
with all the Python dependencies installed.
This will place you inside a Python virtual environment.
DTS is mounted via a volume, which is essentially a symlink from the host to the container.
This enables you to edit and run inside the container
and then delete the container when you are done, keeping your work.
It is also strongly recommended that you mount your SSH keys into the container
to allow you to connect to hosts without specifying a password.

#### Start docker container with SSH keys

```shell
docker build --target dev -t dpdk-dts .
docker run -v $(pwd)/..:/dpdk -v /home/dtsuser/.ssh:/root/.ssh:ro -it dpdk-dts bash
$ poetry install
$ poetry shell
```

#### Start docker container without SSH keys

```shell
docker build --target dev -t dpdk-dts .
docker run -v $(pwd)/..:/dpdk -it dpdk-dts bash
$ poetry install
$ poetry shell
```

### Vim/Emacs

Any editor in the Ubuntu repos should be easy to use,
with Vim and Emacs already installed.
You can add your normal config files as a volume,
enabling you to use your preferred settings.

```shell
docker run -v ${HOME}/.vimrc:/root/.vimrc -v $(pwd)/..:/dpdk -it dpdk-dts bash
```

### Visual Studio Code

VSCode has first-class support for developing with containers.
You may need to run the non-Docker setup commands in the integrated terminal.
DTS contains a .devcontainer config,
so if you open the folder in VSCode it should prompt you to use the dev container
assuming you have the plugin installed.
Please refer to
[VS Development Containers Docs](https://code.visualstudio.com/docs/remote/containers)
to set it all up.
Additionally, there is a line in `.devcontainer/devcontainer.json` that, when included,
will mount the SSH keys of the user currently running VSCode into the container for you.
The `source` on this line can be altered to mount any SSH keys
on the local machine into the container at the correct location.

### Other

Searching for '$IDE dev containers' will probably lead you in the right direction.
