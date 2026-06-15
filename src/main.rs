#![windows_subsystem = "windows"]

use std::os::windows::ffi::OsStrExt;
use windows::core::*;
use windows::Win32::Foundation::*;
use windows::Win32::Media::Audio::Endpoints::*;
use windows::Win32::Media::Audio::*;
use windows::Win32::System::Com::*;
use windows::Win32::System::LibraryLoader::GetModuleHandleW;
use windows::Win32::UI::Shell::*;
use windows::Win32::UI::WindowsAndMessaging::*;
use windows::Win32::UI::Input::KeyboardAndMouse::*;

const TRAY_ICON_ID: u32 = 1;
const TIMER_ID: usize = 101;
const HOTKEY_ID: i32 = 1;

// Custom window messages
const WM_USER_TRAY: u32 = WM_USER + 1;

// Tray menu IDs
const MENU_TOGGLE: usize = 201;
const MENU_EXIT: usize = 202;

static mut HICON_ON: HICON = HICON(std::ptr::null_mut());
static mut HICON_MUTE: HICON = HICON(std::ptr::null_mut());
static mut CURRENT_MUTE_STATE: bool = false;

unsafe fn get_mic_volume_control() -> Result<IAudioEndpointVolume> {
    unsafe {
        let _ = CoInitializeEx(None, COINIT_APARTMENTTHREADED).ok();
        
        let enumerator: IMMDeviceEnumerator = CoCreateInstance(
            &MMDeviceEnumerator,
            None,
            CLSCTX_ALL,
        )?;
        
        let device = enumerator.GetDefaultAudioEndpoint(eCapture, eConsole)?;
        let volume: IAudioEndpointVolume = device.Activate(CLSCTX_ALL, None)?;
        Ok(volume)
    }
}

fn get_mic_mute() -> bool {
    unsafe {
        if let Ok(vol) = get_mic_volume_control() {
            vol.GetMute().map(|b| b.as_bool()).unwrap_or(false)
        } else {
            false
        }
    }
}

fn set_mic_mute(mute: bool) -> Result<()> {
    unsafe {
        let vol = get_mic_volume_control()?;
        vol.SetMute(BOOL::from(mute), &GUID::default())?;
        Ok(())
    }
}

fn copy_to_u16_slice(src: &str, dest: &mut [u16]) {
    let wide: Vec<u16> = src.encode_utf16().chain(Some(0)).collect();
    let len = wide.len().min(dest.len());
    dest[..len].copy_from_slice(&wide[..len]);
}

unsafe fn update_tray_icon(hwnd: HWND, show_notification: bool) {
    unsafe {
        let is_muted = get_mic_mute();
        CURRENT_MUTE_STATE = is_muted;
        
        let hicon = if is_muted { HICON_MUTE } else { HICON_ON };
        let tip = if is_muted { "Microphone Muted" } else { "Microphone On" };
        
        let mut nid = NOTIFYICONDATAW {
            cbSize: std::mem::size_of::<NOTIFYICONDATAW>() as u32,
            hWnd: hwnd,
            uID: TRAY_ICON_ID,
            uFlags: NIF_ICON | NIF_TIP | NIF_MESSAGE,
            uCallbackMessage: WM_USER_TRAY,
            hIcon: hicon,
            ..Default::default()
        };
        
        copy_to_u16_slice(tip, &mut nid.szTip);
        
        let _ = Shell_NotifyIconW(NIM_MODIFY, &nid);
        
        if show_notification {
            let title = "Microphone Status";
            let message = if is_muted { "Microphone is now MUTED" } else { "Microphone is now ACTIVE" };
            
            let mut nid_info = NOTIFYICONDATAW {
                cbSize: std::mem::size_of::<NOTIFYICONDATAW>() as u32,
                hWnd: hwnd,
                uID: TRAY_ICON_ID,
                uFlags: NIF_INFO,
                dwInfoFlags: NIIF_INFO,
                ..Default::default()
            };
            
            copy_to_u16_slice(message, &mut nid_info.szInfo);
            copy_to_u16_slice(title, &mut nid_info.szInfoTitle);
            
            let _ = Shell_NotifyIconW(NIM_MODIFY, &nid_info);
        }
    }
}

unsafe extern "system" fn wnd_proc(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    unsafe {
        match msg {
            WM_CREATE => {
                // Register hotkey: Ctrl + Alt + Space
                let _ = RegisterHotKey(hwnd, HOTKEY_ID, HOT_KEY_MODIFIERS(MOD_CONTROL.0 | MOD_ALT.0), 0x20);
                
                // Set 1-second polling timer
                SetTimer(hwnd, TIMER_ID, 1000, None);
                
                // Add initial tray icon
                let is_muted = get_mic_mute();
                CURRENT_MUTE_STATE = is_muted;
                let hicon = if is_muted { HICON_MUTE } else { HICON_ON };
                let tip = if is_muted { "Microphone Muted" } else { "Microphone On" };
                
                let mut nid = NOTIFYICONDATAW {
                    cbSize: std::mem::size_of::<NOTIFYICONDATAW>() as u32,
                    hWnd: hwnd,
                    uID: TRAY_ICON_ID,
                    uFlags: NIF_ICON | NIF_TIP | NIF_MESSAGE,
                    uCallbackMessage: WM_USER_TRAY,
                    hIcon: hicon,
                    ..Default::default()
                };
                copy_to_u16_slice(tip, &mut nid.szTip);
                let _ = Shell_NotifyIconW(NIM_ADD, &nid);
                
                LRESULT(0)
            }
            WM_HOTKEY => {
                if wparam.0 as i32 == HOTKEY_ID {
                    let current_mute = get_mic_mute();
                    let _ = set_mic_mute(!current_mute);
                    update_tray_icon(hwnd, true);
                }
                LRESULT(0)
            }
            WM_TIMER => {
                if wparam.0 == TIMER_ID {
                    let actual_mute = get_mic_mute();
                    if actual_mute != CURRENT_MUTE_STATE {
                        update_tray_icon(hwnd, true);
                    }
                }
                LRESULT(0)
            }
            WM_USER_TRAY => {
                let event = lparam.0 as u32;
                if event == WM_RBUTTONUP || event == WM_LBUTTONDBLCLK {
                    let menu = CreatePopupMenu().unwrap();
                    let toggle_label = if get_mic_mute() { "Turn Microphone On" } else { "Turn Microphone Off" };
                    let label_w = toggle_label.encode_utf16().chain(Some(0)).collect::<Vec<u16>>();
                    
                    AppendMenuW(menu, MF_STRING, MENU_TOGGLE, PCWSTR::from_raw(label_w.as_ptr())).unwrap();
                    AppendMenuW(menu, MF_SEPARATOR, 0, PCWSTR::null()).unwrap();
                    
                    let quit_label = "Quit";
                    let quit_w = quit_label.encode_utf16().chain(Some(0)).collect::<Vec<u16>>();
                    AppendMenuW(menu, MF_STRING, MENU_EXIT, PCWSTR::from_raw(quit_w.as_ptr())).unwrap();
                    
                    let mut pt = POINT::default();
                    GetCursorPos(&mut pt).unwrap();
                    SetForegroundWindow(hwnd).unwrap();
                    
                    TrackPopupMenu(
                        menu,
                        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                        pt.x,
                        pt.y,
                        0,
                        hwnd,
                        None,
                    ).unwrap();
                    
                    DestroyMenu(menu).unwrap();
                }
                LRESULT(0)
            }
            WM_COMMAND => {
                match wparam.0 {
                    MENU_TOGGLE => {
                        let current_mute = get_mic_mute();
                        let _ = set_mic_mute(!current_mute);
                        update_tray_icon(hwnd, true);
                    }
                    MENU_EXIT => {
                        let _ = DestroyWindow(hwnd);
                    }
                    _ => {}
                }
                LRESULT(0)
            }
            WM_DESTROY => {
                let nid = NOTIFYICONDATAW {
                    cbSize: std::mem::size_of::<NOTIFYICONDATAW>() as u32,
                    hWnd: hwnd,
                    uID: TRAY_ICON_ID,
                    ..Default::default()
                };
                let _ = Shell_NotifyIconW(NIM_DELETE, &nid);
                let _ = UnregisterHotKey(hwnd, HOTKEY_ID);
                let _ = KillTimer(hwnd, TIMER_ID);
                PostQuitMessage(0);
                LRESULT(0)
            }
            _ => DefWindowProcW(hwnd, msg, wparam, lparam),
        }
    }
}

fn main() -> Result<()> {
    unsafe {
        // Initialize COM
        let _ = CoInitializeEx(None, COINIT_APARTMENTTHREADED).ok();
        
        // Write embedded icons to temp directory for LoadImageW to read
        let temp_dir = std::env::temp_dir();
        let on_path = temp_dir.join("mic_on.ico");
        let mute_path = temp_dir.join("mic_mute.ico");
        
        std::fs::write(&on_path, include_bytes!("../resources/on.ico")).unwrap();
        std::fs::write(&mute_path, include_bytes!("../resources/mute.ico")).unwrap();
        
        let on_path_w: Vec<u16> = on_path.as_os_str().encode_wide().chain(Some(0)).collect();
        let mute_path_w: Vec<u16> = mute_path.as_os_str().encode_wide().chain(Some(0)).collect();
        
        HICON_ON = HICON(LoadImageW(
            None,
            PCWSTR::from_raw(on_path_w.as_ptr()),
            IMAGE_ICON,
            0,
            0,
            LR_LOADFROMFILE | LR_DEFAULTSIZE,
        ).unwrap().0);
        
        HICON_MUTE = HICON(LoadImageW(
            None,
            PCWSTR::from_raw(mute_path_w.as_ptr()),
            IMAGE_ICON,
            0,
            0,
            LR_LOADFROMFILE | LR_DEFAULTSIZE,
        ).unwrap().0);
        
        // Create window class
        let class_name = w!("MicIndicatorClass");
        
        // Register window class (using RegisterClassExW as standard in modern windows)
        let wnd_class = WNDCLASSEXW {
            cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
            lpfnWndProc: Some(wnd_proc),
            hInstance: GetModuleHandleW(None).unwrap().into(),
            lpszClassName: class_name,
            ..Default::default()
        };
        
        RegisterClassExW(&wnd_class);
        
        // Create message-only window
        let _hwnd = CreateWindowExW(
            WINDOW_EX_STYLE::default(),
            class_name,
            w!("MicIndicator"),
            WINDOW_STYLE::default(),
            0, 0, 0, 0,
            HWND_MESSAGE,
            HMENU::default(),
            GetModuleHandleW(None).unwrap(),
            None,
        ).unwrap();
        
        // Message Loop
        let mut msg = MSG::default();
        while GetMessageW(&mut msg, None, 0, 0).as_bool() {
            let _ = TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    
    Ok(())
}
