:; if [ -z 0 ]; then
  @echo off
  goto :WINCMD
fi

# Bourne Shell
echo GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD) > .devcontainer/branch.env
exit $?

: Windows CMD
:WINCMD
FOR /F "tokens=*" %%g IN ('git rev-parse --abbrev-ref HEAD') do (echo GIT_BRANCH=%%g > .devcontainer/branch.env)