#!/bin/sh
set -eu

ACTION="${1:-install}"
OPENSEARCH_DASHBOARDS_VERSION="${OPENSEARCH_DASHBOARDS_VERSION:-}"
OPENSEARCH_DASHBOARDS_INSTALL_SECURITY="${OPENSEARCH_DASHBOARDS_INSTALL_SECURITY:-disabled}"
SUDO="${SUDO:-sudo}"
if [ "$(id -u)" -eq 0 ]; then
  SUDO=""
fi

usage() {
  cat <<EOF
Usage: $0 [install|upgrade|uninstall|repo-only]

Install or update OpenSearch Dashboards from the OpenSearch Dashboards 3.x
package repository.

Actions:
  install    Register the repository and install OpenSearch Dashboards.
  upgrade    Register the repository and upgrade OpenSearch Dashboards.
  uninstall  Remove OpenSearch Dashboards. Repository files, configuration, and data are kept.
  repo-only  Register the OpenSearch Dashboards repository only.

Environment:
  OPENSEARCH_DASHBOARDS_VERSION           Version to install, for example 3.7.0.
                                          Default: latest available from the repository.
  OPENSEARCH_DASHBOARDS_INSTALL_SECURITY  disabled or enabled. Default: ${OPENSEARCH_DASHBOARDS_INSTALL_SECURITY}
  SUDO                                    Privilege wrapper. Default: sudo, or empty when run as root.

Examples:
  $0 install
  OPENSEARCH_DASHBOARDS_VERSION=3.7.0 $0 install
  $0 uninstall
  OPENSEARCH_DASHBOARDS_INSTALL_SECURITY=enabled $0 install
EOF
}

case "${ACTION}" in
  -h|--help)
    usage
    exit 0
    ;;
esac

if [ "${ACTION}" != "install" ] && [ "${ACTION}" != "upgrade" ] && [ "${ACTION}" != "uninstall" ] && [ "${ACTION}" != "repo-only" ]; then
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
  run ${SUDO} apt-get install -y lsb-release ca-certificates curl gnupg2
  run ${SUDO} mkdir -p /etc/apt/keyrings
  curl -o- https://artifacts.opensearch.org/publickeys/opensearch-release.pgp |
    run ${SUDO} gpg --dearmor --batch --yes -o /etc/apt/keyrings/opensearch-release-keyring
  echo "deb [signed-by=/etc/apt/keyrings/opensearch-release-keyring] https://artifacts.opensearch.org/releases/bundle/opensearch-dashboards/3.x/apt stable main" |
    run ${SUDO} tee /etc/apt/sources.list.d/opensearch-dashboards-3.x.list >/dev/null
  run ${SUDO} apt-get update
}

install_rpm_repo() {
  run ${SUDO} curl -SL https://artifacts.opensearch.org/releases/bundle/opensearch-dashboards/3.x/opensearch-dashboards-3.x.repo -o /etc/yum.repos.d/opensearch-dashboards-3.x.repo
  if command -v dnf >/dev/null 2>&1; then
    run ${SUDO} dnf clean all
  else
    run ${SUDO} yum clean all
  fi
}

install_env() {
  if [ "${OPENSEARCH_DASHBOARDS_INSTALL_SECURITY}" = "disabled" ]; then
    printf 'DISABLE_SECURITY_DASHBOARDS_PLUGIN=true\n'
  fi
}

run_with_install_env() {
  if [ -n "${SUDO}" ]; then
    run ${SUDO} env $(install_env) "$@"
  else
    run env $(install_env) "$@"
  fi
}

case "${ID:-}" in
  debian|ubuntu)
    if [ "${ACTION}" = "uninstall" ]; then
      run ${SUDO} apt-get remove -y opensearch-dashboards
      exit 0
    fi
    install_deb_repo
    if [ "${ACTION}" = "repo-only" ]; then
      exit 0
    fi
    package="opensearch-dashboards"
    if [ -n "${OPENSEARCH_DASHBOARDS_VERSION}" ]; then
      package="opensearch-dashboards=${OPENSEARCH_DASHBOARDS_VERSION}"
    fi
    if [ "${ACTION}" = "upgrade" ]; then
      run_with_install_env apt-get install --only-upgrade -y "${package}"
    else
      run_with_install_env apt-get install -y "${package}"
    fi
    ;;
  almalinux|rocky|rhel|centos|fedora)
    if [ "${ACTION}" = "uninstall" ]; then
      pm="dnf"
      if ! command -v dnf >/dev/null 2>&1; then
        pm="yum"
      fi
      run ${SUDO} "${pm}" remove -y opensearch-dashboards
      exit 0
    fi
    install_rpm_repo
    if [ "${ACTION}" = "repo-only" ]; then
      exit 0
    fi
    package="opensearch-dashboards"
    if [ -n "${OPENSEARCH_DASHBOARDS_VERSION}" ]; then
      package="opensearch-dashboards-${OPENSEARCH_DASHBOARDS_VERSION}"
    fi
    pm="dnf"
    if ! command -v dnf >/dev/null 2>&1; then
      pm="yum"
    fi
    if [ "${ACTION}" = "upgrade" ]; then
      run_with_install_env "${pm}" upgrade -y "${package}"
    else
      run_with_install_env "${pm}" install -y "${package}"
    fi
    ;;
  *)
    echo "Unsupported distribution ID=${ID:-unknown}" >&2
    exit 1
    ;;
esac
