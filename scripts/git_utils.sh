DARKGRAY='\033[1;30m'
RED='\033[0;31m'
LIGHTRED='\033[1;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
LIGHTPURPLE='\033[1;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
RESET='\033[0m'


git_mikhail_colors() {
  git config --global color.diff.meta yellow
  git config --global color.diff.new  white
  git config --global color.diff.old  cyan
}

[[ $(whoami) == "ios" ]] && git_mikhail_colors
[[ $(whoami) == "a-2" ]] && git_mikhail_colors

git config --global pull.rebase true
git config rebase.autoStash true

cdiff() {
  git diff --color-words=.
}

git_log() {
  git log --graph --abbrev-commit --decorate --format=format:'%C(bold blue)%h%C(reset)%C(bold yellow)%d%C(reset) %C(white)%s%C(reset) %C(dim white)' $@
}

git_graph() {
  git_log --all
}

git_check_squashed_merged() {
  local target=$1
  local branch=$2
  local branch_point=$(git merge-base ${target} ${branch})
  local squashed_commit=$(git commit-tree ${branch}^{tree} -p ${branch_point} -m _)
  local cherry=$(git cherry ${target} ${squashed_commit})
  [[ "${cherry}" == "-"* ]]
}

git_silent() {
  local tmp=$(mktemp)
  local rc=0;
  if ! git $@ > $tmp 2>&1; then rc=$?; cat $tmp; fi
  rm $tmp
  return $?
}

git_short() {
  git rev-parse --short $@
}

git_check_commits_after_merge_on_same_branch() {
  local target=$1
  local branch=$2
  local branch_point=$(git merge-base ${target} ${branch})
  for rev_main in $(git rev-list ${branch_point}..${target}); do
    for rev_branch in $(git rev-list ${branch_point}..${branch}); do 
      git_diff=$(git diff ${rev_branch} ${rev_main} | head -n 1)
      if [[ -z ${git_diff} ]]; then
        branch_tmp=${branch}-auto-rebase
        echo -e "${YELLOW}Rebasing ${branch}${RESET} by cherry-picking on top of ${target}, current commit id $(git_short $branch)"
        git stash 
        git_silent checkout -B ${branch_tmp} ${rev_main}
        git_silent cherry-pick ${rev_branch}...${branch} 
        git_diff_with_rebased=$(git diff ${branch_tmp} ${branch})
        if [[ -z ${git_diff_with_rebased} ]]; then
          git_silent checkout -B ${branch} ${branch_tmp}
          git_silent branch -D ${branch_tmp}
        else
          >&2 echo "ERROR: failed to cherry pick"
        fi
        break 2
      fi
    done
  done
}

git_current_branch() {
  git rev-parse --abbrev-ref HEAD
}

git_remove_merged() {
  local target=origin/main
  current=$(git_current_branch)
  git branch --merged | (grep -v "\*" || true) | xargs -rn 1 git branch -d
  git for-each-ref refs/heads/ "--format=%(refname:short)" | while read branch; do
    if [[ ${branch} != ${current} ]] && git_check_squashed_merged ${target} ${branch}; then 
      git branch -D ${branch}
    else
      git_check_commits_after_merge_on_same_branch ${target} ${branch}
    fi
  done || true
}

git_status() {(
  set -euo pipefail
  git fetch --prune
  [[ $(git_current_branch) != "main" ]] && git update-ref -d refs/heads/main
  git config --global pull.ff only
  git_remove_merged
  git status
  git_log --all -20
)}


nbr() {
  git fetch 
  git checkout -B "$*" origin/main
}

git_rebase() {
  git fetch
  git rebase origin/main --autostash
}
