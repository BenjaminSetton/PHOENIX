param(
    [string]$Generator = 'vs2022'
)

. "$PSScriptRoot\build_common.ps1"

Generate-Project -Generator $Generator
