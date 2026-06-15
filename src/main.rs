#![windows_subsystem = "windows"]

use std::os::windows::ffi::OsStrExt;
use windows::core::*;
use windows::Win32::Foundation::*;
use windows::Win32::Graphics::Gdi::*;
use windows::Win32::Media::Audio::Endpoints::*;
use windows::Win32::Media::Audio::*;
use windows::Win32::System::Com::*;
use windows::Win32::System::LibraryLoader::GetModuleHandleW;
use windows::Win32::System::Registry::*;
use windows::Win32::UI::Shell::*;
use windows::Win32::UI::WindowsAndMessaging::*;
use windows::Win32::UI::Input::KeyboardAndMouse::*;

const TRAY_ICON_ID: u32 = 1;
const TIMER_ID: usize = 101;
const TIMER_HUD_ID: usize = 102;
const HOTKEY_ID: i32 = 1;

// Custom window messages
const WM_USER_TRAY: u32 = WM_USER + 1;
const WM_TRIGGER_HUD: u32 = WM_USER + 2;

// Tray menu IDs
const MENU_TOGGLE: usize = 201;
const MENU_AUTOSTART: usize = 203;
const MENU_EXIT: usize = 202;

static mut HICON_ON_TRAY: HICON = HICON(std::ptr::null_mut());
static mut HICON_MUTE_TRAY: HICON = HICON(std::ptr::null_mut());
static mut HICON_ON_HUD: HICON = HICON(std::ptr::null_mut());
static mut HICON_MUTE_HUD: HICON = HICON(std::ptr::null_mut());

static mut CURRENT_MUTE_STATE: bool = false;
static mut HUD_HWND: HWND = HWND(std::ptr::null_mut());

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

fn is_autostart_enabled() -> bool {
    unsafe {
        let mut hkey = HKEY(std::ptr::null_mut());
        if RegOpenKeyExW(
            HKEY_CURRENT_USER,
            w!("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
            0,
            KEY_READ,
            &mut hkey,
        ).is_ok() {
            let mut val_type = REG_VALUE_TYPE::default();
            let mut val_len = 0;
            let res = RegQueryValueExW(
                hkey,
                w!("MicrophoneIndicator"),
                None,
                Some(&mut val_type),
                None,
                Some(&mut val_len),
            );
            let _ = RegCloseKey(hkey);
            res.is_ok()
        } else {
            false
        }
    }
}

fn set_autostart(enable: bool) -> Result<()> {
    unsafe {
        let mut hkey = HKEY(std::ptr::null_mut());
        RegOpenKeyExW(
            HKEY_CURRENT_USER,
            w!("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
            0,
            KEY_WRITE,
            &mut hkey,
        ).ok()?;
        
        if enable {
            if let Ok(exe_path) = std::env::current_exe() {
                let path_w: Vec<u16> = exe_path.as_os_str().encode_wide().chain(Some(0)).collect();
                let path_bytes = std::slice::from_raw_parts(
                    path_w.as_ptr() as *const u8,
                    path_w.len() * 2,
                );
                RegSetValueExW(
                    hkey,
                    w!("MicrophoneIndicator"),
                    0,
                    REG_SZ,
                    Some(path_bytes),
                ).ok()?;
            }
        } else {
            let _ = RegDeleteValueW(hkey, w!("MicrophoneIndicator"));
        }
        
        let _ = RegCloseKey(hkey);
        Ok(())
    }
}

fn copy_to_u16_slice(src: &str, dest: &mut [u16]) {
    let wide: Vec<u16> = src.encode_utf16().chain(Some(0)).collect();
    let len = wide.len().min(dest.len());
    dest[..len].copy_from_slice(&wide[..len]);
}

unsafe fn update_tray_icon(hwnd: HWND, trigger_visual_hud: bool) {
    unsafe {
        let is_muted = get_mic_mute();
        CURRENT_MUTE_STATE = is_muted;
        
        let hicon = if is_muted { HICON_MUTE_TRAY } else { HICON_ON_TRAY };
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
        
        let hud_hwnd = HUD_HWND;
        if trigger_visual_hud && !hud_hwnd.is_invalid() {
            let _ = SendMessageW(hud_hwnd, WM_TRIGGER_HUD, WPARAM(0), LPARAM(0));
        }
    }
}

unsafe extern "system" fn wnd_proc_hud(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    static mut ANIM_FRAME: i32 = 0;
    unsafe {
        match msg {
            WM_TRIGGER_HUD => {
                ANIM_FRAME = 0;
                // Set initial opacity to 0 using COLORKEY (Magenta 0xFF00FF) and ALPHA
                let _ = SetLayeredWindowAttributes(hwnd, COLORREF(0xFF00FF), 0, LWA_COLORKEY | LWA_ALPHA);
                let _ = ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                let _ = InvalidateRect(hwnd, None, BOOL::from(true));
                let _ = SetTimer(hwnd, TIMER_HUD_ID, 15, None);
                LRESULT(0)
            }
            WM_TIMER => {
                if wparam.0 == TIMER_HUD_ID {
                    ANIM_FRAME += 1;
                    let mut alpha = 0;
                    if ANIM_FRAME <= 5 {
                        alpha = (ANIM_FRAME * 200) / 5;
                    } else if ANIM_FRAME <= 15 {
                        alpha = 200;
                    } else if ANIM_FRAME <= 20 {
                        alpha = ((20 - ANIM_FRAME) * 200) / 5;
                    } else {
                        let _ = KillTimer(hwnd, TIMER_HUD_ID);
                        let _ = ShowWindow(hwnd, SW_HIDE);
                    }
                    let _ = SetLayeredWindowAttributes(hwnd, COLORREF(0xFF00FF), alpha as u8, LWA_COLORKEY | LWA_ALPHA);
                }
                LRESULT(0)
            }
            WM_PAINT => {
                let mut ps = PAINTSTRUCT::default();
                let hdc = BeginPaint(hwnd, &mut ps);
                
                let hdc_mem = CreateCompatibleDC(hdc);
                let hbitmap = CreateCompatibleBitmap(hdc, 160, 160);
                let old_bitmap = SelectObject(hdc_mem, hbitmap);
                
                // 1. Fill entire canvas with Magenta (0xFF00FF) key color
                let key_brush = CreateSolidBrush(COLORREF(0xFF00FF));
                let rect = RECT { left: 0, top: 0, right: 160, bottom: 160 };
                let _ = FillRect(hdc_mem, &rect, key_brush);
                let _ = DeleteObject(key_brush);
                
                // 2. Draw current mic icon directly (making the entire HUD background transparent)
                let is_muted = get_mic_mute();
                let hicon = if is_muted { HICON_MUTE_HUD } else { HICON_ON_HUD };
                let _ = DrawIconEx(
                    hdc_mem,
                    32,
                    32,
                    hicon,
                    96,
                    96,
                    0,
                    None,
                    DI_NORMAL,
                );
                
                let _ = BitBlt(hdc, 0, 0, 160, 160, hdc_mem, 0, 0, SRCCOPY);
                
                let _ = SelectObject(hdc_mem, old_bitmap);
                let _ = DeleteObject(hbitmap);
                let _ = DeleteDC(hdc_mem);
                
                let _ = EndPaint(hwnd, &ps);
                LRESULT(0)
            }
            _ => DefWindowProcW(hwnd, msg, wparam, lparam),
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
                let hicon = if is_muted { HICON_MUTE_TRAY } else { HICON_ON_TRAY };
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
                    
                    // Add Start on Windows menu item with checkbox
                    let autostart_label = "Start with Windows";
                    let autostart_w = autostart_label.encode_utf16().chain(Some(0)).collect::<Vec<u16>>();
                    let autostart_flag = if is_autostart_enabled() { MF_CHECKED } else { MF_UNCHECKED };
                    
                    AppendMenuW(menu, MF_STRING | autostart_flag, MENU_AUTOSTART, PCWSTR::from_raw(autostart_w.as_ptr())).unwrap();
                    
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
                    MENU_AUTOSTART => {
                        let current = is_autostart_enabled();
                        let _ = set_autostart(!current);
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
                let hud_hwnd = HUD_HWND;
                if !hud_hwnd.is_invalid() {
                    let _ = DestroyWindow(hud_hwnd);
                }
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
        
        // Load System Tray Icons (crisp 16x16)
        HICON_ON_TRAY = HICON(LoadImageW(
            None,
            PCWSTR::from_raw(on_path_w.as_ptr()),
            IMAGE_ICON,
            16,
            16,
            LR_LOADFROMFILE,
        ).unwrap().0);
        
        HICON_MUTE_TRAY = HICON(LoadImageW(
            None,
            PCWSTR::from_raw(mute_path_w.as_ptr()),
            IMAGE_ICON,
            16,
            16,
            LR_LOADFROMFILE,
        ).unwrap().0);

        // Load HUD Icons (crisp 96x96)
        HICON_ON_HUD = HICON(LoadImageW(
            None,
            PCWSTR::from_raw(on_path_w.as_ptr()),
            IMAGE_ICON,
            96,
            96,
            LR_LOADFROMFILE,
        ).unwrap().0);
        
        HICON_MUTE_HUD = HICON(LoadImageW(
            None,
            PCWSTR::from_raw(mute_path_w.as_ptr()),
            IMAGE_ICON,
            96,
            96,
            LR_LOADFROMFILE,
        ).unwrap().0);
        
        // Register HUD Class
        let class_name_hud = w!("MicIndicatorHUDClass");
        let wnd_class_hud = WNDCLASSEXW {
            cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
            lpfnWndProc: Some(wnd_proc_hud),
            hInstance: GetModuleHandleW(None).unwrap().into(),
            lpszClassName: class_name_hud,
            ..Default::default()
        };
        RegisterClassExW(&wnd_class_hud);
        
        // Position HUD centered on primary monitor
        let screen_width = GetSystemMetrics(SM_CXSCREEN);
        let screen_height = GetSystemMetrics(SM_CYSCREEN);
        let hud_width = 160;
        let hud_height = 160;
        let x = (screen_width - hud_width) / 2;
        let y = (screen_height - hud_height) / 2;
        
        // Create HUD Window (Layered, Topmost, Transparent to clicks, No Activate)
        HUD_HWND = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            class_name_hud,
            w!("MicIndicatorHUD"),
            WS_POPUP,
            x, y, hud_width, hud_height,
            None,
            HMENU::default(),
            GetModuleHandleW(None).unwrap(),
            None,
        ).unwrap();
        
        // Create window class for Main Tray Window
        let class_name = w!("MicIndicatorClass");
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
