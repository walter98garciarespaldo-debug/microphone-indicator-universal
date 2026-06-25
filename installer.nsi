!define APPNAME "Microphone Indicator"
!define COMPANYNAME "Walter Garcia"
!define DESCRIPTION "Tray indicator to mute/unmute and show microphone status."
!define VERSIONMAJOR 1
!define VERSIONMINOR 0
!define VERSIONBUILD 0

# Specify output installer file name and path
OutFile "releases\microphone-indicator-setup.exe"

# Install directory (Program Files for all-user installation)
InstallDir "$PROGRAMFILES64\MicrophoneIndicator"

# Request administrator privileges (UAC prompt)
RequestExecutionLevel admin

# Set installer icon
Icon "resources\icon.ico"

# UI Pages
Page directory
Page instfiles

Section "Install"
    # Set output path to the installation directory
    SetOutPath "$INSTDIR"
    
    # Copy the main executable
    File "releases\microphone-indicator-windows.exe"
    
    # Store installation folder in registry (Global HKLM)
    WriteRegStr HKLM "Software\MicrophoneIndicator" "Install_Dir" "$INSTDIR"
    
    # Registry keys for Windows Add/Remove Programs (Control Panel - Global)
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MicrophoneIndicator" "DisplayName" "${APPNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MicrophoneIndicator" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MicrophoneIndicator" "DisplayVersion" "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MicrophoneIndicator" "Publisher" "${COMPANYNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MicrophoneIndicator" "DisplayIcon" '"$INSTDIR\microphone-indicator-windows.exe"'
    
    # Create the uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    # Create Start Menu shortcuts (Global for all users)
    SetShellVarContext all
    CreateDirectory "$SMPROGRAMS\Microphone Indicator"
    CreateShortcut "$SMPROGRAMS\Microphone Indicator\Microphone Indicator.lnk" "$INSTDIR\microphone-indicator-windows.exe" "" "$INSTDIR\microphone-indicator-windows.exe" 0
    CreateShortcut "$SMPROGRAMS\Microphone Indicator\Uninstall Microphone Indicator.lnk" "$INSTDIR\uninstall.exe"
    
    # Launch the application after installation
    Exec '"$INSTDIR\microphone-indicator-windows.exe"'
SectionEnd

Section "Uninstall"
    # Terminate any running instances of the app first
    ExecWait 'taskkill /F /IM microphone-indicator-windows.exe'
    
    # Remove files
    Delete "$INSTDIR\microphone-indicator-windows.exe"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"
    
    # Remove Start Menu shortcuts (Global for all users)
    SetShellVarContext all
    Delete "$SMPROGRAMS\Microphone Indicator\Microphone Indicator.lnk"
    Delete "$SMPROGRAMS\Microphone Indicator\Uninstall Microphone Indicator.lnk"
    RMDir "$SMPROGRAMS\Microphone Indicator"
    
    # Remove autostart registry key (per-user)
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "MicrophoneIndicator"
    
    # Remove registry keys (Global HKLM)
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MicrophoneIndicator"
    DeleteRegKey HKLM "Software\MicrophoneIndicator"
SectionEnd
