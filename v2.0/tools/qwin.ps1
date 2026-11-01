# Ejecuta qrun.py con tope de tiempo: si el invitado se queda colgado,
# matamos QEMU y recuperamos igualmente el log serie.
#   .\tools\qwin.ps1 <segundos> <argos de qrun.py...>
param([int]$Tope = 90, [Parameter(ValueFromRemainingArguments = $true)]$Args)
$out = Join-Path $env:TEMP ("qwin-{0}.txt" -f (Get-Random))
# Start-Process -ArgumentList une el array con espacios SIN comillas, asi que
# un argumento con espacios ("hola mundo") se partia en dos. Se citan aqui.
$arglist = @("tools/qrun.py") + ($Args | ForEach-Object {
    if ($_ -match ' ') { '"' + $_ + '"' } else { $_ }
})
$p = Start-Process python -ArgumentList $arglist `
     -PassThru -NoNewWindow -WorkingDirectory (Get-Location).Path `
     -RedirectStandardOutput $out
if (-not $p.WaitForExit($Tope * 1000)) {
    Write-Output "[qwin] TEOUT ${Tope}s: matando QEMU"
    try { $p.Kill() } catch {}
    Get-Process qemu-system-i386 -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}
if (Test-Path $out) { Get-Content $out }
Get-Process qemu-system-i386 -ErrorAction SilentlyContinue | Stop-Process -Force
