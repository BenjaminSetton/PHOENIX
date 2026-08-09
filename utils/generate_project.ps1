param(
    [string]$Generator = 'vs2022',
    [string]$Architecture = 'x86_64'
)

. "$PSScriptRoot\build_common.ps1"

Generate-Project -Generator $Generator -Architecture $Architecture
