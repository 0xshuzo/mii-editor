# Create a local, dedicated SSH identity and start the Wii build container.
# The key has no passphrase so Open Remote - SSH can reconnect unattended.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$keyDirectory = Join-Path $projectRoot '.wii-ssh'
$privateKey = Join-Path $keyDirectory 'id_ed25519'
$publicKey = "$privateKey.pub"
$authorizedKeys = Join-Path $keyDirectory 'authorized_keys'

New-Item -ItemType Directory -Path $keyDirectory -Force | Out-Null

$sshKeygen = (Get-Command ssh-keygen -ErrorAction Stop).Source

if (-not (Test-Path -LiteralPath $privateKey -PathType Leaf)) {
    # Windows PowerShell removes an empty native-command argument. The literal
    # double quotes are therefore needed for ssh-keygen to receive an empty
    # passphrase for its -N option.
    & $sshKeygen -q -t ed25519 -f $privateKey -N '""' -C 'mii-editor Wii development container'
    if ($LASTEXITCODE -ne 0) {
        throw "ssh-keygen failed with exit code $LASTEXITCODE."
    }

    Write-Host "Created a dedicated local SSH key at $privateKey."
}

if (-not (Test-Path -LiteralPath $publicKey -PathType Leaf)) {
    $publicKeyContents = & $sshKeygen -y -f $privateKey
    if ($LASTEXITCODE -ne 0) {
        throw "ssh-keygen failed with exit code $LASTEXITCODE."
    }

    [System.IO.File]::WriteAllText(
        $publicKey,
        ($publicKeyContents.TrimEnd() + [Environment]::NewLine),
        [System.Text.UTF8Encoding]::new($false)
    )
}

Copy-Item -LiteralPath $publicKey -Destination $authorizedKeys -Force

# Windows has no POSIX account IDs to pass through to Docker. The image's
# default developer account uses these IDs, which keeps mounted files usable.
if ([string]::IsNullOrWhiteSpace($env:DEV_UID)) {
    $env:DEV_UID = '1000'
}

if ([string]::IsNullOrWhiteSpace($env:DEV_GID)) {
    $env:DEV_GID = '1000'
}

if ([string]::IsNullOrWhiteSpace($env:WII_SSH_PORT)) {
    $env:WII_SSH_PORT = '2222'
}

$docker = (Get-Command docker -ErrorAction Stop).Source
$nativeErrorPreferenceVariable = Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue
if ($null -ne $nativeErrorPreferenceVariable) {
    $previousNativeErrorPreference = $PSNativeCommandUseErrorActionPreference
    $PSNativeCommandUseErrorActionPreference = $false
}
$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'

try {
    # Docker Desktop may emit harmless host-capability warnings on stderr.
    # Suppress those here; the exit code remains the daemon availability check.
    & $docker info *> $null
    $dockerInfoExitCode = $LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousErrorActionPreference
    if ($null -ne $nativeErrorPreferenceVariable) {
        $PSNativeCommandUseErrorActionPreference = $previousNativeErrorPreference
    }
}

if ($dockerInfoExitCode -ne 0) {
    throw 'Docker Desktop is not running. Start Docker Desktop, wait for its engine to finish starting, then run this script again.'
}

Push-Location $projectRoot
try {
    $nativeErrorPreferenceVariable = Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue
    if ($null -ne $nativeErrorPreferenceVariable) {
        $previousNativeErrorPreference = $PSNativeCommandUseErrorActionPreference
        $PSNativeCommandUseErrorActionPreference = $false
    }
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'

    try {
        & $docker compose -f compose.wii.yml up --build --detach
        $dockerComposeExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
        if ($null -ne $nativeErrorPreferenceVariable) {
            $PSNativeCommandUseErrorActionPreference = $previousNativeErrorPreference
        }
    }

    if ($dockerComposeExitCode -ne 0) {
        throw "docker compose failed with exit code $dockerComposeExitCode."
    }
}
finally {
    Pop-Location
}

$sshIdentityFile = $privateKey.Replace('\', '/')

@"

Wii build container is running. In VSCodium's SSH config, add:

Host mii-editor-wii
    HostName 127.0.0.1
    Port $env:WII_SSH_PORT
    User developer
    IdentityFile $sshIdentityFile
    IdentitiesOnly yes

In VSCodium, use Open Remote - SSH's "Remote - SSH: Connect to Host..."
command and select mii-editor-wii. Open /workspace.
"@ | Write-Host
