#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Download an OCI image from Docker Hub and unpack it as a root tree.

usage: fetch-oci-rootfs.py [--layer-min BYTES] <repo> <tag> <arch> <outdir>
e.g.   fetch-oci-rootfs.py freebsd/freebsd-runtime 14.3 amd64 roots/freebsd

--layer-min only unpacks layers of at least that size (handy for images
that wrap a foreign root tree inside a Linux image).

The FreeBSD project publishes its base system as OCI images, which is
the easiest way to get a FreeBSD userland onto a Linux box.  Only the
Python standard library and tar(1) are needed.
"""
import json
import os
import subprocess
import sys
import tempfile
import urllib.request

ACCEPT = ",".join([
    "application/vnd.oci.image.index.v1+json",
    "application/vnd.oci.image.manifest.v1+json",
    "application/vnd.docker.distribution.manifest.list.v2+json",
    "application/vnd.docker.distribution.manifest.v2+json",
])


def get(url, headers, raw=False):
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=300) as r:
        data = r.read()
    return data if raw else json.loads(data)


def main():
    args = sys.argv[1:]
    layer_min = 0
    if args[0] == "--layer-min":
        layer_min = int(args[1])
        args = args[2:]
    repo, tag, arch, out = args[:4]
    tok = get("https://auth.docker.io/token?service=registry.docker.io"
              "&scope=repository:%s:pull" % repo, {})["token"]
    auth = {"Authorization": "Bearer " + tok}
    reg = "https://registry-1.docker.io/v2/" + repo
    m = get("%s/manifests/%s" % (reg, tag), dict(auth, Accept=ACCEPT))
    if "manifests" in m:
        d = [e["digest"] for e in m["manifests"]
             if e["platform"]["architecture"] == arch][0]
        m = get("%s/manifests/%s" % (reg, d), dict(auth, Accept=ACCEPT))
    os.makedirs(out, exist_ok=True)
    for layer in m["layers"]:
        if layer["size"] < layer_min:
            continue
        print("layer %s (%d bytes)" % (layer["digest"][:19], layer["size"]))
        blob = get("%s/blobs/%s" % (reg, layer["digest"]), auth, raw=True)
        with tempfile.NamedTemporaryFile() as f:
            f.write(blob)
            f.flush()
            subprocess.run(["tar", "-xf", f.name, "-C", out,
                            "--warning=no-unknown-keyword"], check=True)


if __name__ == "__main__":
    main()
