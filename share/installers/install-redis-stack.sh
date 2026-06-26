#!/bin/sh
set -eu

ACTION="${1:-install}"
REDIS_PACKAGE="${REDIS_PACKAGE:-redis-stack-server}"
SUDO="${SUDO:-sudo}"
if [ "$(id -u)" -eq 0 ]; then
  SUDO=""
fi

usage() {
  cat <<EOF
Usage: $0 [install|upgrade|repo-only]

Install or update Redis Stack Server with the host package manager.

Debian/Ubuntu systems use apt-get. RHEL-family systems use dnf, or yum when
dnf is not available. The script writes package repositories and packages into
system-managed locations such as /etc and /usr, so root privileges or sudo are
required.

Actions:
  install    Register the Redis package repository and install the package.
  upgrade    Register the Redis package repository and upgrade the package.
  repo-only  Register the Redis package repository only.

Environment:
  REDIS_PACKAGE  Package name to install. Default: ${REDIS_PACKAGE}
                 redis-stack-server installs Redis server and Redis Stack
                 modules, but not RedisInsight. Use redis-stack only when your
                 repository provides it and you want RedisInsight included.
  SUDO           Privilege wrapper. Default: sudo, or empty when run as root.

Examples:
  $0 install
  REDIS_PACKAGE=redis-stack $0 install
  $0 upgrade
EOF
}

case "${ACTION}" in
  -h|--help)
    usage
    exit 0
    ;;
esac

if [ "${ACTION}" != "install" ] && [ "${ACTION}" != "upgrade" ] && [ "${ACTION}" != "repo-only" ]; then
  usage >&2
  exit 2
fi

if [ -r /etc/os-release ]; then
  . /etc/os-release
else
  echo "/etc/os-release is required" >&2
  exit 1
fi

run() {
  echo "+ $*"
  "$@"
}

install_deb_repo() {
  run ${SUDO} apt-get update
  run ${SUDO} apt-get install -y lsb-release curl gpg ca-certificates
  run ${SUDO} mkdir -p /usr/share/keyrings
  curl -fsSL https://packages.redis.io/gpg | run ${SUDO} gpg --dearmor --batch --yes -o /usr/share/keyrings/redis-archive-keyring.gpg
  run ${SUDO} chmod 644 /usr/share/keyrings/redis-archive-keyring.gpg
  codename="$(. /etc/os-release && printf '%s' "${VERSION_CODENAME:-}")"
  if [ -z "${codename}" ] && command -v lsb_release >/dev/null 2>&1; then
    codename="$(lsb_release -cs)"
  fi
  if [ -z "${codename}" ]; then
    echo "Could not determine Debian/Ubuntu codename" >&2
    exit 1
  fi
  echo "deb [signed-by=/usr/share/keyrings/redis-archive-keyring.gpg] https://packages.redis.io/deb ${codename} main" |
    run ${SUDO} tee /etc/apt/sources.list.d/redis.list >/dev/null
  run ${SUDO} apt-get update
}

install_rpm_repo() {
  major="${VERSION_ID%%.*}"
  case "${ID:-}" in
    almalinux|rocky|rhel|centos|fedora)
      ;;
    *)
      echo "Unsupported RPM distribution ID=${ID:-unknown}; set up the Redis repository manually" >&2
      exit 1
      ;;
  esac
  case "${major}" in
    8|9|10)
      repo_platform="rockylinux${major}"
      ;;
    *)
      echo "Unsupported Redis RPM platform version ${VERSION_ID:-unknown}" >&2
      exit 1
      ;;
  esac
  tmp_key="${TMPDIR:-/tmp}/redis.key"
  curl -fsSL https://packages.redis.io/gpg -o "${tmp_key}"
  run ${SUDO} rpm --import "${tmp_key}"
  {
    echo "[Redis]"
    echo "name=Redis"
    echo "baseurl=https://packages.redis.io/rpm/${repo_platform}"
    echo "enabled=1"
    echo "gpgcheck=1"
  } | run ${SUDO} tee /etc/yum.repos.d/redis.repo >/dev/null
  if command -v dnf >/dev/null 2>&1; then
    run ${SUDO} dnf clean all
  else
    run ${SUDO} yum clean all
  fi
}

case "${ID:-}" in
  debian|ubuntu)
    install_deb_repo
    if [ "${ACTION}" = "repo-only" ]; then
      exit 0
    fi
    if [ "${ACTION}" = "upgrade" ]; then
      run ${SUDO} apt-get install --only-upgrade -y "${REDIS_PACKAGE}"
    else
      run ${SUDO} apt-get install -y "${REDIS_PACKAGE}"
    fi
    ;;
  almalinux|rocky|rhel|centos|fedora)
    install_rpm_repo
    if [ "${ACTION}" = "repo-only" ]; then
      exit 0
    fi
    pm="dnf"
    if ! command -v dnf >/dev/null 2>&1; then
      pm="yum"
    fi
    if [ "${ACTION}" = "upgrade" ]; then
      run ${SUDO} "${pm}" upgrade -y "${REDIS_PACKAGE}"
    else
      run ${SUDO} "${pm}" install -y "${REDIS_PACKAGE}"
    fi
    ;;
  *)
    echo "Unsupported distribution ID=${ID:-unknown}" >&2
    exit 1
    ;;
esac
