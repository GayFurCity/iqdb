# IQDB: Image Query Database System

IQDB is a reverse image search system. It lets you search a database of images
to find images that are visually similar to a given image.

This version of IQDB is a fork of the original IQDB used by https://iqdb.org.
This version powers the reverse image search for [FoxTrove](https://github.com/earlopain/FoxTrove).

# Quickstart

```bash
# Run IQDB in Docker on port 5588. This will create a database file in the current directory called `iqdb.sqlite`.
docker run --rm -it -p 5588:5588 -v $PWD:/mnt evazion/iqdb http 0.0.0.0 5588 /mnt/iqdb.sqlite

# Test that IQDB is running
curl -v http://localhost:5588/status

# Add `test.jpg` to IQDB with ID 1234. You will need to generate a unique ID for every image you add.
curl -F file=@test.jpg http://localhost:5588/images/1234

# Find images visually similar to `test.jpg`.
curl -F file=@test.jpg http://localhost:5588/query
```

# Usage

IQDB is a simple HTTP server with a JSON API. It has commands for adding
images, removing images, and searching for similar images. Image hashes are
stored on disk in an SQLite database.

#### Adding images

To add an image to the database, POST a file to `/images/:id` where `:id` is an
ID number for the image. It is up to you to map them to something useful in your application.

```bash
curl -F file=@test.jpg http://localhost:5588/images/1234
```

```json
{
  "hash": "3fe4c6d513c538413fadbc7235383ab23f97674a40909b92f27ff97af97df980fcfdfd00fd71fd77fd7efdfffe00fe7dfe7ffe80fee7fefeff00ff71ff7aff7fff80ffe7fff1fff4fffa00020008001d009d02830285028803020381038304850701078208000801f97df9fffb7afcfdfd77fe00fe7dfe80fefaff00ff7aff7ffffaffff00030007000e000f0010002000830087008e008f009000a0010c010e018202810283028502860290030203810383058306000b83f67afafdfb7ffcf7fcfefcfffd7dfef3fefafeffff7afffa00030007000e001000200080008400870088008e0090010001030107010e018001810183020d02810282029003030483048d0507050e0680",
  "post_id":1234
}
```

The `signature` is the raw IQDB signature of the image. Two images are similar
if their signatures are similar. The `hash` is the signature encoded as a hex
string.

#### Removing images

To remove an image to the database, do `DELETE /images/:id` where `:id` is the
ID number of the image.

```bash
curl -X DELETE http://localhost:5588/images/1234
```

```json
{ "post_id": 1234 }
```

#### Searching for images

To search for an image, POST a `file` to `/query?limit=N`, where `N` is the
maximum number of results to return (default 10).

```bash
curl -F file=@test.jpg 'http://localhost:5588/query?limit=10'
```

```json
[
  {
    "hash":"3fe4c6d513c538413fadbc7235383ab23f97674a40909b92f27ff97af97df980fcfdfd00fd71fd77fd7efdfffe00fe7dfe7ffe80fee7fefeff00ff71ff7aff7fff80ffe7fff1fff4fffa00020008001d009d02830285028803020381038304850701078208000801f97df9fffb7afcfdfd77fe00fe7dfe80fefaff00ff7aff7ffffaffff00030007000e000f0010002000830087008e008f009000a0010c010e018202810283028502860290030203810383058306000b83f67afafdfb7ffcf7fcfefcfffd7dfef3fefafeffff7afffa00030007000e001000200080008400870088008e0090010001030107010e018001810183020d02810282029003030483048d0507050e0680",
    "post_id":1234,
    "score":100
  }
]
```

The response will contain the top N most similar images. The `score` field is
the similarity rating, from 0 to 100. The `post_id` is the ID of the image,
chosen when you added the image.

You will have to determine a good cutoff score yourself. Generally, 90+ is a
strong match, 70+ is weak match (possibly a false positive), and <50 is no
match.

#### Metrics

`GET /metrics` exposes Prometheus metrics in the standard text exposition
format: HTTP request counts and latency histograms by method/route/status,
and the total number of images in the database.

```bash
curl http://localhost:5588/metrics
```

# Compiling

IQDB requires the following dependencies to build:

* A C++ compiler
* [CMake 3.19+](https://cmake.org/install/)
* [SQLite](https://www.sqlite.org/download.html)
* [Git](https://git-scm.com/downloads)

Run `make` to compile the project. The binary will be at `./build/release/src/iqdb`.

Run `make debug` to compile in debug mode. The binary will be at `./build/debug/src/iqdb`.

You can also run `cmake --preset release` then `cmake --build --preset release
--verbose` to build the project. `make` is simply a wrapper for these commands.

You can run `make docker` to build the docker image.

See the [Dockerfile](./Dockerfile) for an example of which packages to install.

# History

This version of IQDB is a fork of the [Danbooru IQDB](https://github.com/danbooru/iqdb)
with improvements from [evazion](https://github.com/danbooru/iqdb)
which is in turn a fork of the [original IQDB](https://iqdb.org/code),
written by [piespy](mailto:piespy@gmail.com). IQDB is based on code from
[imgSeek](https://sourceforge.net/projects/imgseek/), written by Ricardo
Niederberger Cabral. The IQDB algorithm is based on the paper
[Fast Multiresolution Image Querying](https://grail.cs.washington.edu/projects/query/)
by Charles E. Jacobs, Adam Finkelstein, and David H. Salesin.

IQDB is distributed under the terms of the GNU General Public License. See
[COPYING](./COPYING) for details.

# Further reading

* https://grail.cs.washington.edu/projects/query
* https://grail.cs.washington.edu/projects/query/mrquery.pdf
* https://cliutils.gitlab.io/modern-cmake/
* https://riptutorial.com/cmake
* https://github.com/yhirose/cpp-httplib
* https://hub.docker.com/repository/docker/evazion/iqdb
