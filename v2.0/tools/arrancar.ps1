# Arranca FacilOS en QEMU adaptado a tu pantalla.
# El invitado a 1920x1080 es mas alto que el hueco libre de Windows
# (la barra de tareas tapa la barra del invitado). Este lanzador activa
# el ajuste de imagen (zoom-to-fit) y encaja la ventana en el area util,
# asi se ve todo sin mover nada a mano.
# Uso: .\tools\arrancar.ps1 [-Iso ruta] [-Memoria 1G] [-Qemu ruta]

param(
    [string]$Iso = "",
    [string]$Memoria = "1G",
    [string]$Qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
)

Add-Type -AssemblyName System.Windows.Forms
$area = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
Write-Output ("Pantalla util: {0}x{1}" -f $area.Width, $area.Height)

if (-not $Iso) {
    $Iso = Get-ChildItem (Join-Path (Get-Location) "build/iso/*.iso") |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not (Test-Path $Iso)) { Write-Output "No hay ISO: $Iso"; exit 1 }
if (-not (Test-Path $Qemu)) { Write-Output "No hay QEMU: $Qemu"; exit 1 }
Write-Output "ISO: $Iso"

$codigo = @"
using System;
using System.Runtime.InteropServices;
public static class Encaje {
    [DllImport("user32.dll")]
    public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hh, bool r);
}
"@
Add-Type -TypeDefinition $codigo -Language CSharp

$proc = Start-Process $Qemu -ArgumentList @(
    "-cdrom", $Iso, "-boot", "d", "-m", $Memoria,
    "-display", "gtk,zoom-to-fit=on", "-name", "FacilOS"
) -PassThru
Write-Output ("QEMU pid={0}: encajando en {1}x{2}..." -f $proc.Id, $area.Width, $area.Height)

$ventana = [IntPtr]::Zero
for ($i = 0; $i -lt 100 -and $ventana -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 300
    if ($proc.HasExited) { Write-Output "QEMU termino antes de abrir ventana."; exit 1 }
    $proc.Refresh()
    $ventana = $proc.MainWindowHandle
}
if ($ventana -eq [IntPtr]::Zero) {
    Write-Output "Sin ventana visible; se deja como esta (el ajuste sigue activo)."
    exit 0
}
[Encaje]::MoveWindow($ventana, $area.X, $area.Y, $area.Width, $area.Height, $true) | Out-Null
Write-Output "Listo: todo el escritorio a la vista (la imagen se escala sola)."
