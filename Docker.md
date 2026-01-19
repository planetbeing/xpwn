# Docker Usage Instructions

### Setup

Container images are available on [the GitHub Container Registry](https://github.com/orgs/malus-security/packages/container/package/xpwn) (and soon on the GitLab Container Registry).

You can also build your own image locally:

```sh
docker build -t <container_name> .
```

### Usage

The docker image is designed to run `xpwn` in the context of [iExtractor](https://github.com/malus-security/iextractor) and has the following default usage:

```sh
docker run -v <in_file>:/in -v <out_file>:/out -e IV=<iv> -e KEY=<key> -t <container_name>

# runs this command: ./xpwntool /in /out -iv $IV -k $KEY -decrypt
```

However, it is possible to override the default flags:

```sh
docker run -t <container_name> --help
``
