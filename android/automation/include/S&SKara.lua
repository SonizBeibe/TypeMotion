-- ==========================================
-- Metadatos del script
-- ==========================================
script_name = "Kara"
script_description = "Helper interactivo para subtítulos (Standalone)"
script_author = "Soniz & Snutert"
script_version = "3.1"

-- ==========================================
-- Diccionario de Idiomas (Sistema de Localización)
-- ==========================================
local loc = {
    es = {
        sel_func = "⚙ Seleccionar Función:", info_script = "ℹ Información del Script:",
        opt_1 = "{\\k} Cascada", opt_2 = "{\\k} Brillante", opt_3 = "{\\k} Reverso", opt_4 = "FadeWorks", opt_5 = "Ytchroma",
        opt_6 = "Gradiente", opt_7 = "RGB Plus", opt_8 = "{\\move} Recolocator", opt_9 = "{\\k} Cascada Despedida",
        btn_exec = "▶ Ejecutar Macro", btn_info = "ℹ Información", btn_lang = "🌐 Idioma", btn_close = "✕ Cerrar",
        desc_gen = "Este plugin permite procesar subtítulos de manera avanzada.\n\nSelecciona una de las opciones del menú superior para aplicar \nefectos personalizados a las líneas seleccionadas.\n\n• Versión: 3.1 Standalone\n• Estado: Listo para usar",
        desc_1 = "Efecto {\\k} Cascada:\nCrea nuevas líneas simulando una aparición fluida usando \\move.\nPermite elegir múltiples direcciones (Vertical y Horizontal).\nUsa Time-Slicing para evitar cruce de bordes y respeta colores inline.\n\n• Versión: 3.0 Standalone",
        desc_2 = "Efecto {\\k} Brillante:\nCrea un karaoke donde la sílaba activa brilla y hace un fundido (\\t)\nhacia los colores base elegidos o los colores nativos del estilo.\n\n• Versión: 3.1 Standalone",
        desc_3 = "Efecto {\\k} Reverso:\nDesvanece el color primario y deja el secundario como principal. Incluye AegiFade para crear karaokes de desvanecimiento múltiple.\n\n• Versión: 3.0 Standalone",
        desc_4 = "Efecto FadeWorks:\nAplica un desvanecimiento general (\\fad) a la línea, o uno progresivo letra por letra.\n\n• Versión: 3.0 Standalone",
        desc_5 = "Efecto Ytchroma:\nReplica fielmente la aberración cromática del YTSubConverter.\nRespeta y preserva los colores inline de tu texto original en la capa central.\n\n• Versión: 3.0 Standalone",
        desc_6 = "Efecto Gradiente:\nCrea degradados estáticos o animaciones RGB fluidas inyectando tags \\t y \\c carácter por carácter.\n\n• Versión: 3.0 Standalone",
        desc_7 = "Efecto RGB Plus:\nGenera una onda de colores profesional dividiendo la leyenda en micro-segmentos temporales con control de Brillo.\n\n• Versión: 3.0 Standalone",
        desc_8 = "Efecto {\\move} Recolocator:\nAutomatiza animaciones de movimiento detectando el canvas de forma inteligente.\n\n• Versión: 3.0 Standalone",
        desc_9 = "Efecto {\\k} Cascada Despedida (Outro):\nCrea una animación de salida para las sílabas usando \\move y fundido.\nLas letras abandonan su posición original según el tiempo de las sílabas.\n\n• Versión: 3.0 Standalone",
        
        c_title = "┌─ Configuración de Cascada ─────────────────────┐", 
        cd_title= "┌─ Configuración de Cascada Despedida ───────────┐", 
        c_footer = "└─────────────────────────────────────────────┘",
        c_posx = " Posición X (Centro):", c_posy = " Posición Y (Suelo):", c_dist = " Distancia (px):", c_vel = " Velocidad Máx (ms):",
        c_chk1 = "[✎] Mostrar solo color primario", c_chk2 = "[💾] Guardar Reciente",
        c_btn_gen = "✔ Generar Cascada", c_btn_back = "↩ Volver al Menú",
        
        b_title = "                      ◈ CONFIGURACIÓN KARAOKE BRILLANTE ◈",
        b_line1 = "┌──────────────────────────────────────────────┐",
        b_auto = " [🎨] Auto Colors (Usa colores del Estilo)", b_glow = " ☼ Brillo (Glow):",
        b_c1 = " 1️⃣ Primario:", b_c2 = " 2️⃣ Secundario:", b_c3 = " 3️⃣ Borde:", b_c4 = " 4️⃣ Sombra:",
        b_alp = " Alpha:", b_btn_gen = "✔ Aplicar Brillante",
        
        -- Textos importados de dav.lua para el menú Brillante
        sub_normal = "◈ CONFIGURACIÓN: VARIANTE NORMAL ◈",
        sub_inverso = "◈ CONFIGURACIÓN: VARIANTE INVERSO ◈",
        sub_desap = "◈ CONFIGURACIÓN: VARIANTE DESAPARICIÓN ◈",
        m_var = "Selecciona Variante:",
        b_style_out = " 👻 Segundo Estilo (Reemplazo):",
        
        r_title = "                       ◈ CONFIGURACIÓN KARAOKE REVERSO ◈",
        r_auto = " [🎨] Auto Colors (Usar colores del Estilo)",
        r_ytkt = "[✨] Añadir tag \\ytktFade al inicio", 
        r_aegi = "[👁] AegiFade (Múltiples líneas, sin borde/sombra)",
        r_btn_gen = "✔ Aplicar Reverso",
        
        f_title = "                          ◈ CONFIGURACIÓN FADEWORKS ◈",
        f_in = " Fade in (ms):", f_out = " Fade out (ms):", f_ltr = " Por letra (ms):", f_mode = " Dirección:",
        f_chk_prim = "[✎] Solo Color Primario (Usa \\1a en vez de \\alpha)",
        f_btn_gen = "✔ Aplicar Fade", f_btn_letra = "🔠 Por Letra",
        m_ltr = "LTR (Izq -> Der)", m_rtl = "RTL (Der -> Izq)", m_c2s = "Centro -> Lados", m_s2c = "Lados -> Centro",
        m_utd = "UTD (Arriba -> Abajo)", m_dtu = "DTU (Abajo -> Arriba)",
        
        yc_title = "                 ✨ EFEITO YTCHROMA EXATO ✨",
        yc_sec1 = " 📐 [ TIEMPO Y DISTANCIA ]",
        yc_tin = "   Entrada (ms):", yc_tout = "   Salida (ms):", 
        yc_ox = "   Desfase X:", yc_oy = "   Desfase Y:",
        yc_sec2 = " 🎨 [ ESTILO Y CANALES RGB ]",
        yc_auto = " [🎨] Auto Colors", yc_glow = " [✨] Ativar Brillo", yc_chaos = " [🌪️] Modo Caos",
        yc_yt = " [✨] Tag {\\ytchroma}",
        yc_c1 = " Capa A (Der):", yc_c2 = " Capa B (Cen):", yc_c3 = " Capa C (Izq):",
        yc_a1 = " Alpha A:", yc_a2 = " Alpha B:", yc_a3 = " Alpha C:",
        yc_save = " [💾] Guardar configuración", yc_btn_gen = "✔ Generar Ytchroma",

        gr_title = "                          ◈ CONFIGURACIÓN GRADIENTE ◈",
        gr_hsl = " Interpolar HSL", gr_app = " Aplicar a:", gr_start = " Inicio", gr_end = " Fin",
        gr_prim = " Primario:", gr_bord = " Borde:", gr_shad = " Sombra:",
        gr_rgb_t = " Animación RGB:", gr_rgb_on = " Activar RGB", gr_cyc = " Ciclos:", gr_thk = " Borde:",
        gr_btn = "✔ Aplicar Gradiente",
        
        rp_title = "                             ◈ CONFIGURACIÓN RGB PLUS ◈",
        rp_int = " Fragmentos (ms):", rp_spd = " Vel. Onda:", rp_glow = " Int. Brillo:", 
        rp_man = " Usar Colores Manuales", rp_tgt = " Aplicar a:", rp_btn = "✔ Aplicar RGB Plus",
        
        mv_title = "                     ◈ CONFIGURAÇÃO {\\move} RECOLOCATOR ◈",
        mv_dist = " Distancia (px):", mv_dur = " Velocidad (ms):", mv_dir = " Dirección:",
        mv_out = " Modo Saída (Invierte viaje)",
        mv_btn = "✔ Aplicar Move",
        
        lang_title = "🌐 Selecciona el idioma:", lang_ok = "✔ Aplicar", lang_cancel = "✕ Cancelar"
    }
}
loc.en = loc.es
loc.pt = loc.es

-- ==========================================
-- Configuración Patreon (Memoria de Sesión)
-- ==========================================
local CLAVE_SECRETA = "123" 
local LINK_PATREON = "https://www.patreon.com/cw/Snutert_and_soniz"
local sesion_validada = false 

local function verificar_acceso_patreon()
    if sesion_validada then return true end

    local ui_bloqueo = {
        {class="label", label="╔════════════════════════════════════════════════════════════════╗", x=0, y=0, width=2},
        {class="label", label="     S&SFX KARA SUITE | ACCESO RESTRINGIDO", x=0, y=1, width=2},
        {class="label", label="╚════════════════════════════════════════════════════════════════╝", x=0, y=2, width=2},
        {class="label", label=" ", x=0, y=3},
        {class="label", label=" ¡Hola! Este módulo es exclusivo para los Patreons.", x=0, y=4, width=2},
        {class="label", label=" Ingresa la clave de acceso de tu tier:", x=0, y=5, width=2},
        {class="textbox", name="user_key", value="", x=0, y=6, width=2, height=2},
        {class="label", label=" ", x=0, y=8},
        {class="label", label="  Consigue tu clave en: " .. LINK_PATREON, x=0, y=9, width=2},
    }

    local btn, res = aegisub.dialog.display(ui_bloqueo, {"▶ Desbloquear", "✖ Cancelar"})
    if btn == "▶ Desbloquear" then
        if res.user_key:match(CLAVE_SECRETA) then
            sesion_validada = true 
            return true
        else
            aegisub.dialog.display({
                {class="label", label=" CLAVE INCORRECTA.", x=0, y=0},
                {class="label", label="Por favor revisa tu cuenta de Patreon o copia bien el texto.", x=0, y=1}
            }, {"Cerrar"})
            return false
        end
    end
    return false
end

-- ==========================================
-- Sistema de Guardado de Configuración
-- ==========================================
local config_file = aegisub.decode_path("?user/") .. "yt_kara_cfg.txt"

local function cargar_config()
    local cfg = {
        pos_x = 640, pos_y = 710, altura = 40, velocidad = 300, c_dir_idx = 1, solo_primario = true, guardar = true, lang = "es",
        
        -- Configuración de Brillante (Importada de dav.lua)
        variante = "1. Normal",
        n_b_auto = true, n_col_glow = "#00FF00", n_col_1 = "#98ADC6", n_col_2 = "#364052", n_col_3 = "#364052", n_col_4 = "#364052", n_alp_1 = "&H00&", n_alp_2 = "&H00&", n_alp_3 = "&H00&", n_alp_4 = "&H00&",
        i_b_auto = true, i_col_glow = "#FF0000", i_col_1 = "#98ADC6", i_col_2 = "#364052", i_col_3 = "#364052", i_col_4 = "#364052", i_alp_1 = "&H00&", i_alp_2 = "&H00&", i_alp_3 = "&H00&", i_alp_4 = "&H00&",
        d_b_auto = true, d_col_glow = "#00FFFF", d_col_1 = "#98ADC6", d_col_2 = "#364052", d_col_3 = "#364052", d_col_4 = "#364052", d_alp_1 = "&H00&", d_alp_2 = "&H00&", d_alp_3 = "&H00&", d_alp_4 = "&H00&", d_estilo_out = "Default",
        
        r_auto = true, rev_col_2 = "#7A4DFD", rev_col_3 = "#000000", rev_col_4 = "#000000",
        rev_alp_2 = "&H00&", rev_alp_3 = "&H00&", rev_alp_4 = "&H00&", rev_ytkt = true, rev_aegi = false,
        fw_in = 0, fw_out = 0, fw_ltr = "120", fw_mode_idx = 1, fw_prim = false,
        
        yc_tin = 300, yc_tout = 300, yc_ox = 18.4, yc_oy = 20,
        yc_auto = false, yc_chaos = false, yc_glow = false, yc_yt = false,
        yc_c1 = "#0000FF", yc_c2 = "#00FF00", yc_c3 = "#FF0000",
        yc_a1 = "&HDB&", yc_a2 = "&HA0&", yc_a3 = "&H85&",
        
        gr_hsl = false, gr_app_c = true, gr_app_3c = true, gr_app_4c = true, 
        gr_c1 = "#FF0000", gr_b1 = "#400000", gr_s1 = "#000000",
        gr_c5 = "#FF00FF", gr_b5 = "#400040", gr_s5 = "#000000",
        gr_u2 = false, gr_c2 = "#FFFF00", gr_b2 = "#404000", gr_s2 = "#000000",
        gr_u3 = false, gr_c3 = "#00FF00", gr_b3 = "#004000", gr_s3 = "#000000",
        gr_u4 = false, gr_c4 = "#0000FF", gr_b4 = "#000040", gr_s4 = "#000000",
        gr_rgb_on = false, gr_speed = 1.0, gr_sz = 2.0,
        rf_fatias = 10, rf_onda = 500, rf_man = false, 
        rf_k1 = "#FF0000", rf_k2 = "#FFFF00", rf_k3 = "#00FF00", rf_k4 = "#00FFFF",
        rf_c1 = true, rf_c3 = false, rf_c4 = false, rp_glow = 0.8,
        mv_pos_x = 640, mv_pos_y = 360, mv_dist = 620, mv_dur = 300, mv_dir_idx = 1, mv_out = false
    }
    local file = io.open(config_file, "r")
    if file then
        for line in file:lines() do
            local k, v = string.match(line, "([^=]+)=(.+)")
            if k and v then
                if k:match("^[nid]_col_") or k:match("^[nid]_alp_") or k:match("^col_") or k:match("^alp_") or k:match("^rev_col_") or k:match("^rev_alp_") or 
                   k:match("^yc_c") or k:match("^yc_a%d") or k:match("^gr_c") or k:match("^gr_b") or k:match("^gr_s") or
                   k:match("^rf_k") or k == "fw_ltr" or k == "variante" or k == "d_estilo_out" then cfg[k] = v
                elseif v == "true" then cfg[k] = true
                elseif v == "false" then cfg[k] = false
                elseif tonumber(v) then cfg[k] = tonumber(v)
                else cfg[k] = v end
            end
        end
        file:close()
    end
    return cfg
end

local function guardar_config(cfg)
    local file = io.open(config_file, "w")
    if file then
        for k, v in pairs(cfg) do
            if cfg.guardar or k == "lang" then 
                file:write(k .. "=" .. tostring(v) .. "\n")
            end
        end
        file:close()
    end
end

-- ==========================================
-- Funciones Matemáticas Universales y Lectura de Lienzo
-- ==========================================
local function clonar_linea(linea)
    local nueva = {}
    for k, v in pairs(linea) do nueva[k] = v end
    return nueva
end

local function get_clean_chars(text)
    local chars = {}
    local clean = text:gsub("{.-}","")
    for char in clean:gmatch("[%z\1-\127\194-\244][\128-\191]*") do table.insert(chars, char) end
    return chars
end

local function get_line_pos(line, styles, playres_x, playres_y)
    local style = styles[line.style] or {}
    local align = tonumber(line.text:match("^{\\[^}]*\\an(%d)")) or tonumber(line.text:match("\\an(%d)")) or style.align or 2
    local px, py = line.text:match("\\pos%(([%d%.%-]+)%s*,%s*([%d%.%-]+)%)")
    if px and py then return tonumber(px), tonumber(py), align end
    
    local ml = (line.margin_l and line.margin_l > 0) and line.margin_l or (style.margin_l or 0)
    local mr = (line.margin_r and line.margin_r > 0) and line.margin_r or (style.margin_r or 0)
    local mt = (line.margin_t and line.margin_t > 0) and line.margin_t or (style.margin_t or style.margin_v or 0)
    local mb = (line.margin_b and line.margin_b > 0) and line.margin_b or (style.margin_b or style.margin_v or 0)
    
    local x = playres_x / 2
    if align == 1 or align == 4 or align == 7 then x = ml
    elseif align == 3 or align == 6 or align == 9 then x = playres_x - mr end
    
    local y = playres_y - mb
    if align == 4 or align == 5 or align == 6 then y = playres_y / 2
    elseif align == 7 or align == 8 or align == 9 then y = mt end
    return x, y, align
end

local function hex_to_rgb_math(hex)
    hex = hex:gsub("[#&H]","")
    if #hex == 6 then return tonumber(hex:sub(1,2),16) or 255, tonumber(hex:sub(3,4),16) or 255, tonumber(hex:sub(5,6),16) or 255 end
    return 255, 255, 255
end

local function rgb_to_ass_math(r, g, b)
    r, g, b = math.max(0, math.min(255, math.floor(r))), math.max(0, math.min(255, math.floor(g))), math.max(0, math.min(255, math.floor(b)))
    return string.format("&H%02X%02X%02X&", b, g, r)
end

local function hsv_to_ass_math(h, s, v)
    local r, g, b
    local i = math.floor(h * 6); local f = h * 6 - i
    local p, q, t = v*(1-s), v*(1-f*s), v*(1-(1-f)*s)
    i = i % 6
    if i == 0 then r,g,b = v,t,p elseif i == 1 then r,g,b = q,v,p elseif i == 2 then r,g,b = p,v,t
    elseif i == 3 then r,g,b = p,q,v elseif i == 4 then r,g,b = t,p,v elseif i == 5 then r,g,b = v,p,q end
    return rgb_to_ass_math(r*255, g*255, b*255)
end

local function interpolate_multi(t, colors, use_hsl)
    if #colors == 1 then return rgb_to_ass_math(hex_to_rgb_math(colors[1])) end
    local seg = 1 / (#colors - 1)
    local idx = math.min(math.floor(t / seg) + 1, #colors - 1)
    local lt = (t - (idx - 1) * seg) / seg
    local r1, g1, b1 = hex_to_rgb_math(colors[idx])
    local r2, g2, b2 = hex_to_rgb_math(colors[idx+1])
    return rgb_to_ass_math(r1+(r2-r1)*lt, g1+(g2-g1)*lt, b1+(b2-b1)*lt)
end

local function blend_math(c1, pct, glow_int)
    local b1, g1, r1 = c1:match("&H(%x%x)(%x%x)(%x%x)&")
    if not b1 then return c1 end
    r1, g1, b1 = tonumber(r1,16), tonumber(g1,16), tonumber(b1,16)
    local factor = pct * glow_int
    local r = r1 + (255 - r1) * factor
    local g = g1 + (255 - g1) * factor
    local b = b1 + (255 - b1) * factor
    return string.format("&H%02X%02X%02X&", math.floor(b), math.floor(g), math.floor(r))
end

local function hsv_to_ass_plus(h)
    local r, g, b
    local i = math.floor(h * 6); local f = h * 6 - i
    local p, q, t = 0, 1 - f, f
    i = i % 6
    if i == 0 then r, g, b = 1, t, 0 elseif i == 1 then r, g, b = q, 1, 0 elseif i == 2 then r, g, b = 0, 1, t
    elseif i == 3 then r, g, b = 0, q, 1 elseif i == 4 then r, g, b = t, 0, 1 elseif i == 5 then r, g, b = 1, 0, q end
    return string.format("&H%02X%02X%02X&", math.floor(b*255), math.floor(g*255), math.floor(r*255))
end

local function hex_to_ass(hex)
    if not hex then return "&HFFFFFF&" end
    local r, g, b = string.match(hex, "#(%x%x)(%x%x)(%x%x)")
    if r and g and b then return string.format("&H%s%s%s&", b, g, r) end
    return hex
end

local function get_style_color(style_name, color_key, fallback_hex, styles, auto)
    if not auto then return hex_to_ass(fallback_hex) end
    local s = styles[style_name]
    if s and s[color_key] then
        local hex = s[color_key]:match("&H%x%x(%x%x%x%x%x%x)")
        if hex then return "&H" .. hex .. "&" end
    end
    return hex_to_ass(fallback_hex)
end

local function clean_slashes(str)
    return str:gsub("\\+", "\\")
end

-- ==========================================
-- PARSEADOR DE SÍLABAS INLINE (SEGURO PARA COLORES)
-- ==========================================
local function extract_syllables_safe(text)
    local silabas = {}
    local i = 1
    while i <= #text do
        local s, e, dur = text:find("{\\k(%d+)}", i)
        if not s then
            local remain = text:sub(i)
            if #silabas > 0 then 
                silabas[#silabas].texto = silabas[#silabas].texto .. remain 
            elseif remain ~= "" then
                table.insert(silabas, {duracion = 0, texto = remain})
            end
            break
        end
        
        local pre = ""
        if s > i then pre = text:sub(i, s - 1) end
        
        local next_s = text:find("{\\k%d+}", e + 1)
        local content = text:sub(e + 1, (next_s and next_s - 1) or -1)
        
        if #silabas > 0 then
            silabas[#silabas].texto = silabas[#silabas].texto .. pre
        else
            content = pre .. content
        end
        
        table.insert(silabas, {duracion = tonumber(dur) * 10, texto = content})
        i = next_s or (#text + 1)
    end
    return silabas
end

-- ==========================================
-- Lógicas de Efectos (Procesamiento)
-- ==========================================

function procesar_cascada(subs, sel, cfg, styles, playres_x, playres_y, is_manual)
    local lineas_insertadas = 0
    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]
        
        local bx, by, align = get_line_pos(line, styles, playres_x, playres_y)
        if is_manual then bx, by = cfg.pos_x, cfg.pos_y end
        
        local clean_text = line.text:gsub("\\pos%([^%)]+%)", ""):gsub("\\move%([^%)]+%)", ""):gsub("\\an%d", "")
        local silabas = extract_syllables_safe(clean_text)
        
        if #silabas > 0 then
            local tiempo_acumulado = 0
            local offset_local = 1
            
            local x_inicio = bx
            local y_inicio = by
            local dir = cfg.c_dir_idx or 1
            
            if dir == 1 then y_inicio = by - cfg.altura      -- Arriba -> Abajo
            elseif dir == 2 then y_inicio = by + cfg.altura  -- Abajo -> Arriba
            elseif dir == 3 then x_inicio = bx - cfg.altura  -- Izquierda -> Derecha
            elseif dir == 4 then x_inicio = bx + cfg.altura  -- Derecha -> Izquierda
            end
            
            local tag_hide = cfg.solo_primario and "{\\1a&HFF&}" or "{\\alpha&HFF&}"
            local tag_show = cfg.solo_primario and "{\\1a&H00&}" or "{\\alpha&H00&}"
            local mov_extra = cfg.solo_primario and "\\3a&HFF&\\4a&HFF&" or ""
            
            for s = 1, #silabas do
                local t_start = line.start_time + tiempo_acumulado
                local dur = silabas[s].duracion
                local t_end = t_start + dur
                
                local vel = math.min(cfg.velocidad, dur)
                if s == #silabas then
                    vel = math.min(cfg.velocidad, math.floor(dur / 2))
                end
                
                local x_dest = bx
                local y_dest = by
                
                if dir == 5 then
                    local n = #silabas
                    x_inicio = bx
                    y_inicio = by
                    x_dest = (s <= math.ceil(n / 2)) and (bx - cfg.altura) or (bx + cfg.altura)
                    y_dest = by
                elseif dir == 6 then
                    local n = #silabas
                    x_inicio = (s <= math.ceil(n / 2)) and (bx - cfg.altura) or (bx + cfg.altura)
                    y_inicio = by
                    x_dest = bx
                    y_dest = by
                end
                
                local mov_line = clonar_linea(line)
                mov_line.start_time = t_start
                mov_line.end_time = t_end
                
                local mov_text = string.format("{\\an%d\\move(%.1f,%.1f,%.1f,%.1f,0,%d)%s}", align, x_inicio, y_inicio, x_dest, y_dest, vel, mov_extra)
                for j = 1, #silabas do
                    mov_text = mov_text .. ((j == s) and tag_show or tag_hide) .. silabas[j].texto
                end
                
                mov_line.text = mov_text:gsub("}{", "")
                mov_line.effect = "Cascada Movimiento"
                subs[-(indice_real + offset_local)] = mov_line
                offset_local = offset_local + 1
                
                if s > 1 then
                    local stat_line = clonar_linea(line)
                    stat_line.start_time = t_start
                    stat_line.end_time = t_end
                    
                    local stat_text = string.format("{\\an%d\\pos(%.1f,%.1f)}", align, bx, by)
                    for j = 1, #silabas do
                        stat_text = stat_text .. ((j < s) and tag_show or tag_hide) .. silabas[j].texto
                    end
                    
                    stat_line.text = stat_text:gsub("}{", "")
                    stat_line.effect = "Cascada Base"
                    subs[-(indice_real + offset_local)] = stat_line
                    offset_local = offset_local + 1
                end
                
                tiempo_acumulado = tiempo_acumulado + dur
            end
            
            local final_start = line.start_time + tiempo_acumulado
            if final_start < line.end_time then
                local final_line = clonar_linea(line)
                final_line.start_time = final_start
                final_line.end_time = line.end_time
                
                local final_text = string.format("{\\an%d\\pos(%.1f,%.1f)}", align, bx, by)
                for j = 1, #silabas do
                    final_text = final_text .. tag_show .. silabas[j].texto
                end
                
                final_line.text = final_text:gsub("}{", "")
                final_line.effect = "Cascada Final"
                subs[-(indice_real + offset_local)] = final_line
                offset_local = offset_local + 1
            end
            
            line.comment = true
            subs[indice_real] = line
            lineas_insertadas = lineas_insertadas + (offset_local - 1)
        end
    end
    aegisub.set_undo_point("Efecto Cascada")
end

function procesar_cascada_despedida(subs, sel, cfg, styles, playres_x, playres_y, is_manual)
    local lineas_insertadas = 0
    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]
        
        local bx, by, align = get_line_pos(line, styles, playres_x, playres_y)
        if is_manual then bx, by = cfg.pos_x, cfg.pos_y end
        
        local clean_text = line.text:gsub("\\pos%([^%)]+%)", ""):gsub("\\move%([^%)]+%)", ""):gsub("\\an%d", "")
        local silabas = extract_syllables_safe(clean_text)
        
        if #silabas > 0 then
            local tiempo_acumulado = 0
            local offset_local = 1
            local dir = cfg.c_dir_idx or 1
            
            local tag_hide = cfg.solo_primario and "{\\1a&HFF&}" or "{\\alpha&HFF&}"
            local tag_show = cfg.solo_primario and "{\\1a&H00&}" or "{\\alpha&H00&}"
            
            for s = 1, #silabas do
                local t_start = line.start_time + tiempo_acumulado
                local dur = silabas[s].duracion
                local t_end = t_start + dur
                
                local vel = math.min(cfg.velocidad, dur)
                if s == #silabas then
                    vel = math.min(cfg.velocidad, math.floor(dur / 2))
                end
                
                local x_dest = bx
                local y_dest = by
                
                if dir == 1 then y_dest = by + cfg.altura      -- Arriba -> Abajo (Cae)
                elseif dir == 2 then y_dest = by - cfg.altura  -- Abajo -> Arriba (Sube)
                elseif dir == 3 then x_dest = bx + cfg.altura  -- Izquierda -> Derecha
                elseif dir == 4 then x_dest = bx - cfg.altura  -- Derecha -> Izquierda
                elseif dir == 5 then 
                    x_dest = (s <= math.ceil(#silabas / 2)) and (bx - cfg.altura) or (bx + cfg.altura)
                elseif dir == 6 then 
                    x_dest = (s <= math.ceil(#silabas / 2)) and (bx + cfg.altura) or (bx - cfg.altura)
                end
                
                if t_start > line.start_time then
                    local stat_line = clonar_linea(line)
                    stat_line.start_time = line.start_time
                    stat_line.end_time = t_start
                    
                    local stat_text = string.format("{\\an%d\\pos(%.1f,%.1f)}", align, bx, by)
                    for j = 1, #silabas do
                        stat_text = stat_text .. ((j == s) and tag_show or tag_hide) .. silabas[j].texto
                    end
                    
                    stat_line.text = stat_text:gsub("}{", "")
                    stat_line.effect = "Cascada Outro Base"
                    subs[-(indice_real + offset_local)] = stat_line
                    offset_local = offset_local + 1
                end
                
                local mov_line = clonar_linea(line)
                mov_line.start_time = t_start
                mov_line.end_time = t_start + vel
                if mov_line.end_time < t_end then mov_line.end_time = t_end end
                
                local tag_fade_out = string.format("\\t(0,%d,%s)", vel, tag_hide:gsub("[{}]", ""))
                
                local mov_text = string.format("{\\an%d\\move(%.1f,%.1f,%.1f,%.1f,0,%d)%s}", align, bx, by, x_dest, y_dest, vel, tag_fade_out)
                for j = 1, #silabas do
                    mov_text = mov_text .. ((j == s) and tag_show or tag_hide) .. silabas[j].texto
                end
                
                mov_line.text = mov_text:gsub("}{", "")
                mov_line.effect = "Cascada Outro Mov"
                subs[-(indice_real + offset_local)] = mov_line
                offset_local = offset_local + 1
                
                tiempo_acumulado = tiempo_acumulado + dur
            end
            
            line.comment = true
            subs[indice_real] = line
            lineas_insertadas = lineas_insertadas + (offset_local - 1)
        end
    end
    aegisub.set_undo_point("Efecto Cascada Despedida")
end

function procesar_brillante(subs, sel, cfg, styles)
    local lineas_insertadas = 0
    local modo = cfg.variante or "1. Normal"

    -- Mapeo de prefijo según la variante activa
    local prefix = "n_"
    if modo == "2. Inverso" then prefix = "i_"
    elseif modo == "3. Desaparición" then prefix = "d_" end

    local v_b_auto = cfg[prefix .. "b_auto"]
    local v_col_glow = cfg[prefix .. "col_glow"]
    local v_col_1 = cfg[prefix .. "col_1"]
    local v_col_2 = cfg[prefix .. "col_2"]
    local v_col_3 = cfg[prefix .. "col_3"]
    local v_col_4 = cfg[prefix .. "col_4"]
    local v_alp_1 = cfg[prefix .. "alp_1"]
    local v_alp_2 = cfg[prefix .. "alp_2"]
    local v_alp_3 = cfg[prefix .. "alp_3"]
    local v_alp_4 = cfg[prefix .. "alp_4"]
    local v_estilo_out = cfg.d_estilo_out or "Default"

    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]
        local offset_local = 1
        
        local pos_tag = line.text:match("\\pos%([^%)]+%)") or ""
        local an_tag = line.text:match("\\an%d") or ""
        
        local ass_1c = get_style_color(line.style, "color1", v_col_1, styles, v_b_auto)
        local ass_2c = get_style_color(line.style, "color2", v_col_2, styles, v_b_auto)
        local ass_3c = get_style_color(line.style, "color3", v_col_3, styles, v_b_auto)
        local ass_4c = get_style_color(line.style, "color4", v_col_4, styles, v_b_auto)
        local glow = hex_to_ass(v_col_glow)
        
        local tag_global = string.format("{%s%s\\1c%s\\2c%s\\3c%s\\4c%s\\1a%s\\2a%s\\3a%s\\4a%s}", pos_tag, an_tag, ass_1c, ass_2c, ass_3c, ass_4c, v_alp_1, v_alp_2, v_alp_3, v_alp_4)

        local clean_text = line.text:gsub("\\pos%([^%)]+%)", ""):gsub("\\move%([^%)]+%)", ""):gsub("\\an%d", "")
        local silabas = extract_syllables_safe(clean_text)
        
        if #silabas > 0 then
            local tiempo_acumulado = 0
            
            local start_idx, end_idx, step
            if modo == "1. Normal" or modo == "3. Desaparición" then
                start_idx = 1
                end_idx = #silabas
                step = 1
            else
                start_idx = #silabas
                end_idx = 1
                step = -1
            end
            
            for s = start_idx, end_idx, step do
                local nueva_linea = clonar_linea(line)
                nueva_linea.start_time = line.start_time + tiempo_acumulado
                nueva_linea.end_time = nueva_linea.start_time + silabas[s].duracion
                nueva_linea.effect = "Pro - " .. modo
                local texto_final = ""
                
                for j = 1, #silabas do
                    local t_silaba = silabas[j].texto
                    local base_tag = (j == 1 and tag_global or "")
                    local dur_animacion = silabas[j].duracion
                    
                    if (modo ~= "2. Inverso" and s == #silabas) or (modo == "2. Inverso" and s == 1) then
                        dur_animacion = math.floor(dur_animacion / 2)
                    end
                    
                    if modo == "1. Normal" then
                        if j < s then
                            texto_final = texto_final .. base_tag .. string.format("{\\alpha&H00&\\1c%s\\3c%s\\4c%s}", ass_1c, ass_3c, ass_4c) .. t_silaba
                        elseif j == s then
                            texto_final = texto_final .. base_tag .. string.format("{\\alpha&H00&\\1c%s\\3c%s\\4c%s\\t(0,%d,\\1c%s\\3c%s\\4c%s)}", glow, glow, glow, dur_animacion, ass_1c, ass_3c, ass_4c) .. t_silaba
                        else
                            texto_final = texto_final .. base_tag .. "{\\alpha&HFF&}" .. t_silaba
                        end
                        
                    elseif modo == "2. Inverso" then
                        if j > s then
                            texto_final = texto_final .. base_tag .. string.format("{\\alpha&H00&\\1c%s\\3c%s\\4c%s}", ass_1c, ass_3c, ass_4c) .. t_silaba
                        elseif j == s then
                            texto_final = texto_final .. base_tag .. string.format("{\\alpha&H00&\\1c%s\\3c%s\\4c%s\\t(0,%d,\\1c%s\\3c%s\\4c%s)}", glow, glow, glow, dur_animacion, ass_1c, ass_3c, ass_4c) .. t_silaba
                        else
                            texto_final = texto_final .. base_tag .. "{\\alpha&HFF&}" .. t_silaba
                        end
                        
                    elseif modo == "3. Desaparición" then
                        if j < s - 1 then
                            -- FASE 3: Sílabas más antiguas desaparecen por completo
                            if j == 1 then
                                texto_final = texto_final .. string.format("{%s%s\\alpha&HFF&}", pos_tag, an_tag) .. t_silaba
                            else
                                texto_final = texto_final .. "{\\alpha&HFF&}" .. t_silaba
                            end
                        elseif j == s - 1 then
                            -- FASE 2: Sílaba anterior pasa al segundo estilo a media transparencia (\alpha&H80&)
                            local tag = string.format("{\\r%s%s%s\\alpha&H80&}", v_estilo_out, pos_tag, an_tag)
                            texto_final = texto_final .. tag .. t_silaba
                        elseif j == s then
                            -- FASE 1: Sílaba activa animando de brillo a color primario
                            -- Forzamos el reset al estilo original para limpiar el \r previo
                            local tag = string.format("{\\r%s%s%s\\alpha&H00&\\1c%s\\2c%s\\3c%s\\4c%s\\1a%s\\2a%s\\3a%s\\4a%s\\1c%s\\3c%s\\4c%s\\t(0,%d,\\1c%s\\3c%s\\4c%s)}", 
                                line.style, pos_tag, an_tag, 
                                ass_1c, ass_2c, ass_3c, ass_4c, 
                                v_alp_1, v_alp_2, v_alp_3, v_alp_4,
                                glow, glow, glow, dur_animacion, ass_1c, ass_3c, ass_4c)
                            texto_final = texto_final .. tag .. t_silaba
                        else
                            -- Sílabas futuras: Esperando su turno con visibilidad normal
                            texto_final = texto_final .. string.format("{\\alpha&H00&\\1c%s\\2c%s\\3c%s\\4c%s}", ass_1c, ass_2c, ass_3c, ass_4c) .. t_silaba
                        end
                    end
                end
                
                nueva_linea.text = texto_final:gsub("}{", "")
                subs[-(indice_real + offset_local)] = nueva_linea
                offset_local = offset_local + 1
                lineas_insertadas = lineas_insertadas + 1
                tiempo_acumulado = tiempo_acumulado + silabas[s].duracion
            end
            line.comment = true
            subs[indice_real] = line
        end
    end
    aegisub.set_undo_point("Karaoke Pro Avanzado")
end

function procesar_reverso(subs, sel, cfg, styles)
    local lineas_insertadas = 0

    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]
        local offset_local = 1
        
        local c2 = get_style_color(line.style, "color2", cfg.rev_col_2, styles, cfg.r_auto)
        local c3 = get_style_color(line.style, "color3", cfg.rev_col_3, styles, cfg.r_auto)
        local c4 = get_style_color(line.style, "color4", cfg.rev_col_4, styles, cfg.r_auto)
        
        if cfg.rev_aegi then
            local clean_text = line.text:gsub("\\pos%([^%)]+%)", ""):gsub("\\move%([^%)]+%)", ""):gsub("\\an%d", "")
            local silabas = extract_syllables_safe(clean_text)
            
            if #silabas > 0 then
                local tiempo_acumulado = 0
                for s = 1, #silabas do
                    local nueva_linea = clonar_linea(line)
                    nueva_linea.start_time = line.start_time + tiempo_acumulado
                    nueva_linea.end_time = nueva_linea.start_time + silabas[s].duracion
                    nueva_linea.effect = "ReverseK"
                    local texto_final = ""
                    local tag_global = string.format("{\\2c%s\\3c%s\\4c%s\\2a&HFF&\\3a&HFF&\\4a&HFF&}", c2, c3, c4)
                    for j = 1, #silabas do
                        local t_silaba = silabas[j].texto
                        local prefix = (j == 1) and tag_global or ""
                        if j < s then
                            texto_final = texto_final .. prefix .. "{\\1a&HFF&}" .. t_silaba
                        elseif j == s then
                            local dur_animacion = silabas[j].duracion
                            if s == #silabas then
                                dur_animacion = math.floor(dur_animacion / 2)
                            end
                            texto_final = texto_final .. prefix .. string.format("{\\1a&H00&\\t(0,%d,\\1a&HFF&)}", dur_animacion) .. t_silaba
                        else
                            texto_final = texto_final .. prefix .. "{\\1a&H00&}" .. t_silaba
                        end
                    end
                    nueva_linea.text = texto_final:gsub("}{", "")
                    subs[-(indice_real + offset_local)] = nueva_linea
                    offset_local = offset_local + 1
                    lineas_insertadas = lineas_insertadas + 1
                    tiempo_acumulado = tiempo_acumulado + silabas[s].duracion
                end
                line.comment = true
                subs[indice_real] = line
            end
        else
            local ytkt_tag = cfg.rev_ytkt and "\\ytktFade" or ""
            local tag_reverso = string.format("\\1a&HFF&\\2a%s\\2c%s\\3c%s\\3a%s\\4c%s\\4a%s%s", 
                cfg.rev_alp_2, c2, c3, cfg.rev_alp_3, c4, cfg.rev_alp_4, ytkt_tag)
            local nueva_linea = clonar_linea(line)
            if nueva_linea.text:match("^{") then
                nueva_linea.text = nueva_linea.text:gsub("^{", "{" .. tag_reverso)
            else
                nueva_linea.text = "{" .. tag_reverso .. "}" .. nueva_linea.text
            end
            nueva_linea.effect = "Karaoke Reverso"
            line.comment = true
            subs[indice_real] = line
            subs[-(indice_real + offset_local)] = nueva_linea
            lineas_insertadas = lineas_insertadas + 1
        end
    end
    aegisub.set_undo_point("Efecto Reverso")
end

function procesar_fadeworks_linea(subs, sel, cfg)
    local lineas_insertadas = 0
    local f_in = tonumber(cfg.fw_in) or 0
    local f_out = tonumber(cfg.fw_out) or 0
    local tag_fad = string.format("\\fad(%d,%d)", f_in, f_out)
    
    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]
        local offset_local = 1
        local nueva_linea = clonar_linea(line)
        if nueva_linea.text:match("^{") then
            nueva_linea.text = nueva_linea.text:gsub("^{", "{" .. tag_fad)
        else
            nueva_linea.text = "{" .. tag_fad .. "}" .. nueva_linea.text
        end
        nueva_linea.effect = "Fadeworks Normal"
        line.comment = true
        subs[indice_real] = line
        subs[-(indice_real + offset_local)] = nueva_linea
        lineas_insertadas = lineas_insertadas + 1
    end
    aegisub.set_undo_point("Efecto FadeWorks (Línea)")
end

function procesar_fadeworks_letra(subs, sel, cfg)
    local f_in = tonumber(cfg.fw_in) or 0
    local f_out = tonumber(cfg.fw_out) or 0
    local f_ltr = tonumber(cfg.fw_ltr) or 120
    local mode_idx = cfg.fw_mode_idx
    local tag_alpha = cfg.fw_prim and "\\1a" or "\\alpha"
    
    local lineas_insertadas = 0
    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]
        local offset_local = 1
        local clean_text = line.text:gsub("{[^}]+}", "")
        local chars = {}
        for c in string.gmatch(clean_text, "[%z\1-\127\194-\244][\128-\191]*") do
            table.insert(chars, c)
        end
        
        local num_chars = #chars
        if num_chars > 0 then
            local out_text = ""
            local center = (num_chars - 1) / 2
            local line_dur = line.end_time - line.start_time
            
            for idx = 0, num_chars - 1 do
                local char = chars[idx + 1]
                local factor = 0
                
                if mode_idx == 1 then factor = idx / math.max(1, num_chars - 1)
                elseif mode_idx == 2 then factor = 1 - (idx / math.max(1, num_chars - 1))
                elseif mode_idx == 3 then factor = math.abs(idx - center) / math.max(1, center)
                elseif mode_idx == 4 then factor = 1 - (math.abs(idx - center) / math.max(1, center)) end
                
                local tags = ""
                if f_in > 0 then
                    local spread_in = math.max(0, f_in - f_ltr)
                    local start_in = math.floor(factor * spread_in)
                    tags = tags .. string.format("\\t(%d,%d,1,%s&H00&)", start_in, start_in + f_ltr, tag_alpha)
                end
                if f_out > 0 then
                    local spread_out = math.max(0, f_out - f_ltr)
                    local start_out = line_dur - f_out + math.floor(factor * spread_out)
                    tags = tags .. string.format("\\t(%d,%d,1,%s&HFF&)", start_out, start_out + f_ltr, tag_alpha)
                end
                
                if tags ~= "" then
                    local prefix = (f_in > 0) and (tag_alpha .. "&HFF&") or ""
                    out_text = out_text .. string.format("{%s%s}%s", prefix, tags, char)
                else out_text = out_text .. char end
            end
            
            local nueva_linea = clonar_linea(line)
            nueva_linea.text = out_text:gsub("}{", "")
            if mode_idx == 1 then nueva_linea.effect = "Fadeworks LTR"
            elseif mode_idx == 2 then nueva_linea.effect = "Fadeworks RTL"
            elseif mode_idx == 3 then nueva_linea.effect = "Fadeworks Centro a Lados"
            elseif mode_idx == 4 then nueva_linea.effect = "Fadeworks Lados a Centro" end
            
            line.comment = true
            subs[indice_real] = line
            subs[-(indice_real + offset_local)] = nueva_linea
            lineas_insertadas = lineas_insertadas + 1
        end
    end
    aegisub.set_undo_point("Efecto FadeWorks (Letra)")
end

function procesar_ytchroma(subs, sel, cfg, styles, playres_x, playres_y)
    local lineas_insertadas = 0
    local step = 33 
    local layer_dur = 66 

    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]
        local offset_local = 1 
        
        local bx, by, align = get_line_pos(line, styles, playres_x, playres_y)
        
        local ox = tonumber(cfg.yc_ox) or 18.4
        local oy = tonumber(cfg.yc_oy) or 20
        local tin = tonumber(cfg.yc_tin) or 300
        local tout = tonumber(cfg.yc_tout) or 300
        
        local c1 = get_style_color(line.style, "color3", cfg.yc_c1, styles, cfg.yc_auto)
        local c2 = get_style_color(line.style, "color1", cfg.yc_c2, styles, cfg.yc_auto)
        local c3 = get_style_color(line.style, "color2", cfg.yc_c3, styles, cfg.yc_auto)
        
        local a1 = cfg.yc_a1 or "&HDB&"
        local a2 = cfg.yc_a2 or "&HA0&"
        local a3 = cfg.yc_a3 or "&H85&"
        
        local text_no_pos = line.text:gsub("\\pos%([^%)]+%)", ""):gsub("\\move%([^%)]+%)", ""):gsub("\\an%d", "")
        
        local text_chroma = text_no_pos:gsub("\\[1234]?c&H%x+&", ""):gsub("\\[1234]?a&H%x+&", ""):gsub("\\alpha&H%x+&", ""):gsub("\\blur[%d%.]+", "")
        
        local t_start = line.start_time
        local t_end = line.end_time
        local dur = t_end - t_start
        
        local function add_layer(st, et, tags, txt, suffix)
            if st >= et then return end
            local nl = clonar_linea(line)
            nl.start_time = st
            nl.end_time = et
            
            if txt:match("^{") then
                nl.text = txt:gsub("^{", "{" .. clean_slashes(tags))
            else
                nl.text = "{" .. clean_slashes(tags) .. "}" .. txt
            end
            
            nl.effect = "Ytchroma " .. suffix
            subs[-(indice_real + offset_local)] = nl
            offset_local = offset_local + 1
            lineas_insertadas = lineas_insertadas + 1
        end

        if cfg.yc_yt then
            local tag_yt = string.format("\\ytchroma(%s,%s,%s,%s,%.1f,%.1f,%d,%d)", c1, c2, c3, a1, ox, oy, tin, tout)
            local txt_clean = text_no_pos:gsub("\\ytchroma%([^%)]*%)", "")
            add_layer(t_start, t_end, tag_yt, txt_clean, "Tag")
        
        elseif cfg.yc_chaos then
            local i_count = 0
            local c_arr = {c1, c2, c3}
            local a_arr = {a1, a2, a3}
            
            local dir_x = {-1, 0, 1, 0, -1, 1, -1, 1}
            local dir_y = {-1, 0, 1, -1, 1, 0, 1, -1}
            
            for t = t_start, t_end - 1, step do
                local et = math.min(t + layer_dur, t_end)
                
                local rand_idx = math.random(1, 8)
                local rx_mult = dir_x[rand_idx]
                local ry_mult = dir_y[rand_idx]
                
                local r_ox = rx_mult * ox
                local r_oy = ry_mult * oy
                
                local cc = c_arr[(i_count % 3) + 1]
                local aa = a_arr[(i_count % 3) + 1]
                
                local tag_chaos = string.format("\\an%d\\pos(%.3f,%.3f)\\bord0\\shad0\\1c%s\\1a%s\\", align, bx + r_ox, by + r_oy, cc, aa)
                add_layer(t, et, tag_chaos, text_chroma, "Chaos")
                
                if cfg.yc_glow then
                    local tag_glow = string.format("\\an%d\\pos(%.3f,%.3f)\\blur3\\3c%s\\3a&H80&\\1c%s\\1a&H50&\\bord5\\", align, bx + r_ox, by + r_oy, c2, c2)
                    add_layer(t, et, tag_glow, text_chroma, "Glow")
                end
                
                add_layer(t, et, string.format("\\an%d\\pos(%.3f,%.3f)\\", align, bx, by), text_no_pos, "Base")
                
                i_count = i_count + 1
            end
            
        else
            if tin + tout > dur then
                tin = dur / 2
                tout = dur / 2
            end
            
            local mid_start = t_start + tin
            local mid_end = t_end - tout
            
            -- FASE 1: Fade IN
            for t = t_start, mid_start - 1, step do
                local et = math.min(t + layer_dur, mid_start + layer_dur) 
                local p = (t - t_start) / tin
                local cur_ox = ox * p
                local cur_oy = oy * p
                
                if cfg.yc_glow then
                    local tag_glow = string.format("\\an%d\\pos(%.3f,%.3f)\\blur3\\3c%s\\3a&H80&\\1c%s\\1a&H50&\\bord5\\", align, bx, by, c2, c2)
                    add_layer(t, et, tag_glow, text_chroma, "In Glow")
                end
                
                local tag_l = string.format("\\an%d\\bord0\\shad0\\pos(%.3f,%.3f)\\1c%s\\1a%s\\", align, bx - cur_ox, by - cur_oy, c3, a3)
                add_layer(t, et, tag_l, text_chroma, "In L")
                
                local tag_r = string.format("\\an%d\\bord0\\shad0\\pos(%.3f,%.3f)\\1c%s\\1a%s\\", align, bx + cur_ox, by + cur_oy, c1, a1)
                add_layer(t, et, tag_r, text_chroma, "In R")
                
                local tag_c = string.format("\\an%d\\pos(%.3f,%.3f)\\", align, bx, by)
                add_layer(t, et, tag_c, text_no_pos, "In C")
            end
            
            -- FASE 2: Medio 
            if mid_end > mid_start then
                local tag_mid = string.format("\\an%d\\pos(%.3f,%.3f)\\", align, bx, by)
                add_layer(mid_start, mid_end, tag_mid, text_no_pos, "Medio")
            end
            
            -- FASE 3: Fade OUT
            for t = mid_end, t_end - 1, step do
                local et = math.min(t + layer_dur, t_end)
                local p = 1 - ((t - mid_end) / tout)
                local cur_ox = ox * p
                local cur_oy = oy * p
                
                if cfg.yc_glow then
                    local tag_glow = string.format("\\an%d\\pos(%.3f,%.3f)\\blur3\\3c%s\\3a&H80&\\1c%s\\1a&H50&\\bord5\\", align, bx, by, c2, c2)
                    add_layer(t, et, tag_glow, text_chroma, "Out Glow")
                end
                
                local tag_l = string.format("\\an%d\\bord0\\shad0\\pos(%.3f,%.3f)\\1c%s\\1a%s\\", align, bx - cur_ox, by - cur_oy, c3, a3)
                add_layer(t, et, tag_l, text_chroma, "Out L")
                
                local tag_r = string.format("\\an%d\\bord0\\shad0\\pos(%.3f,%.3f)\\1c%s\\1a%s\\", align, bx + cur_ox, by + cur_oy, c1, a1)
                add_layer(t, et, tag_r, text_chroma, "Out R")
                
                local tag_c = string.format("\\an%d\\pos(%.3f,%.3f)\\", align, bx, by)
                add_layer(t, et, tag_c, text_no_pos, "Out C")
            end
        end
        
        line.comment = true
        subs[indice_real] = line
    end
    aegisub.set_undo_point("Efecto Ytchroma Exacto")
end

function procesar_gradiente(subs, sel, cfg)
    local lineas_insertadas = 0
    for idx = 1, #sel do
        local i = sel[idx] + lineas_insertadas
        local line = subs[i]
        local offset_local = 1
        
        local all_tags = ""
        for t in line.text:gmatch("{[^}]*}") do all_tags = all_tags .. t:sub(2,-2) end
        local chars = get_clean_chars(line.text)
        local out = ""
        local dur = line.end_time - line.start_time

        for c=1, #chars do
            local t = (#chars > 1) and (c-1)/(#chars-1) or 0
            local tags = "{" .. all_tags
            if cfg.gr_rgb_on then
                local color_in = hsv_to_ass_math(t, 0.9, 1)
                local color_out = hsv_to_ass_math((t + cfg.gr_speed) % 1, 0.9, 1)
                tags = tags .. string.format("\\c%s\\t(0,%d,\\c%s)", color_in, dur, color_out)
            else
                local cl, bl, sl = {cfg.gr_c1}, {cfg.gr_b1}, {cfg.gr_s1}
                if cfg.gr_u2 then table.insert(cl, cfg.gr_c2) table.insert(bl, cfg.gr_b2) table.insert(sl, cfg.gr_s2) end
                if cfg.gr_u3 then table.insert(cl, cfg.gr_c3) table.insert(bl, cfg.gr_b3) table.insert(sl, cfg.gr_s3) end
                if cfg.gr_u4 then table.insert(cl, cfg.gr_c4) table.insert(bl, cfg.gr_b4) table.insert(sl, cfg.gr_s4) end
                table.insert(cl, cfg.gr_c5) table.insert(bl, cfg.gr_b5) table.insert(sl, cfg.gr_s5)
                
                if cfg.gr_app_c then tags = tags .. "\\c" .. interpolate_multi(t, cl, cfg.gr_hsl) end
                if cfg.gr_app_3c then tags = tags .. "\\3c" .. interpolate_multi(t, bl, cfg.gr_hsl) end
                if cfg.gr_app_4c then tags = tags .. "\\4c" .. interpolate_multi(t, sl, cfg.gr_hsl) end
            end
            out = out .. tags .. "\\bord" .. cfg.gr_sz .. "}" .. chars[c]
        end
        
        local nueva_linea = clonar_linea(line)
        nueva_linea.text = out
        nueva_linea.effect = "Gradiente RGB"
        line.comment = true
        subs[i] = line
        subs[-(i + offset_local)] = nueva_linea
        lineas_insertadas = lineas_insertadas + 1
    end
    aegisub.set_undo_point("Efecto Gradiente")
end

function procesar_rgb_plus(subs, sel, cfg)
    local lineas_insertadas = 0
    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]
        local clean_text = line.text:gsub("{[^}]*}", "")
        local tags = line.text:match("^{[^}]*}") or "{}"
        local colors = {hex_to_ass(cfg.rf_k1), hex_to_ass(cfg.rf_k2), hex_to_ass(cfg.rf_k3), hex_to_ass(cfg.rf_k4)}
        
        local offset_local = 1
        for t = line.start_time, line.end_time - 1, cfg.rf_fatias do
            local nl = clonar_linea(line)
            nl.start_time = t
            nl.end_time = math.min(t + cfg.rf_fatias, line.end_time)
            nl.effect = "RGB Plus"
            
            local out = ""
            local ci = 0
            for char in clean_text:gmatch("[%z\1-\127\194-\244][\128-\191]*") do
                ci = ci + 1
                local move = ((t - line.start_time) / cfg.rf_onda) + (ci * 0.08)
                local base = cfg.rf_man and colors[math.floor(move * 4) % 4 + 1] or hsv_to_ass_plus(move % 1)
                local final = blend_math(base, math.sin(move * 6.28) * 0.5 + 0.5, cfg.rp_glow)
                
                local ctags = ""
                if cfg.rf_c1 then ctags = ctags .. "\\c" .. final end
                if cfg.rf_c3 then ctags = ctags .. "\\3c" .. final end
                if cfg.rf_c4 then ctags = ctags .. "\\4c" .. final end
                out = out .. "{" .. ctags .. "}" .. char
            end
            nl.text = tags:gsub("}$", "") .. out:gsub("^{", "")
            subs[-(indice_real + offset_local)] = nl
            offset_local = offset_local + 1
            lineas_insertadas = lineas_insertadas + 1
        end
        line.comment = true
        subs[indice_real] = line
    end
    aegisub.set_undo_point("Efecto RGB Plus")
end

function procesar_recolocator(subs, sel, cfg, styles, playres_x, playres_y, is_manual)
    local lineas_insertadas = 0
    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]
        local offset_local = 1
        
        local bx, by, align = get_line_pos(line, styles, playres_x, playres_y)
        if is_manual then bx, by = cfg.mv_pos_x, cfg.mv_pos_y end

        local start_x, start_y = bx, by
        local end_x, end_y = bx, by
        local dist = tonumber(cfg.mv_dist) or 200
        local dur = tonumber(cfg.mv_dur) or 300
        
        if cfg.mv_dir_idx == 1 then start_x = bx - dist
        elseif cfg.mv_dir_idx == 2 then start_x = bx + dist
        elseif cfg.mv_dir_idx == 3 then start_y = by - dist
        elseif cfg.mv_dir_idx == 4 then start_y = by + dist end

        if cfg.mv_out then
            local temp_x, temp_y = start_x, start_y
            start_x, start_y = end_x, end_y
            end_x, end_y = temp_x, temp_y
        end

        local tag_move = string.format("\\an%d\\move(%.1f,%.1f,%.1f,%.1f,0,%d)", align, start_x, start_y, end_x, end_y, dur)
        
        local nueva_linea = clonar_linea(line)
        local clean_text = nueva_linea.text:gsub("\\pos%([%d%.%-]+%s*,%s*[%d%.%-]+%)", "")
        clean_text = clean_text:gsub("\\move%([%d%.%-]+%s*,%s*[%d%.%-]+%s*,%s*[%d%.%-]+%s*,%s*[%d%.%-]+[^%)]*%)", "")
        clean_text = clean_text:gsub("\\an%d", "")
        
        if clean_text:match("^{") then clean_text = clean_text:gsub("^{", "{" .. tag_move)
        else clean_text = "{" .. tag_move .. "}" .. clean_text end
        clean_text = clean_text:gsub("{%s*}", "")

        nueva_linea.text = clean_text
        nueva_linea.effect = "Move Recolocator"
        
        line.comment = true
        subs[indice_real] = line
        subs[-(indice_real + offset_local)] = nueva_linea
        lineas_insertadas = lineas_insertadas + 1
    end
    aegisub.set_undo_point("Efecto Recolocator")
end

-- ==========================================
-- Sub-Menús de Interfaz
-- ==========================================
function menu_idioma(cfg, t)
    local num_lang = "Español (es)"
    if cfg.lang == "en" then num_lang = "English (en)"
    elseif cfg.lang == "pt" then num_lang = "Português (pt-BR)" end

    local dialog = {
        {class = "label", x = 0, y = 0, width = 2, label = t.lang_title},
        {class = "dropdown", name = "sel_idioma", x = 0, y = 1, width = 2, items = {"Español (es)", "English (en)", "Português (pt-BR)"}, value = num_lang}
    }
    
    local btn, res = aegisub.dialog.display(dialog, {t.lang_ok, t.lang_cancel})
    if btn == t.lang_ok then
        if res.sel_idioma == "Español (es)" then cfg.lang = "es"
        elseif res.sel_idioma == "English (en)" then cfg.lang = "en"
        elseif res.sel_idioma == "Português (pt-BR)" then cfg.lang = "pt" end
        guardar_config(cfg)
    end
end

function menu_cascada(subs, sel, cfg, t, styles, playres_x, playres_y)
    local def_x, def_y = 640, 710
    if #sel > 0 then
        def_x, def_y, _ = get_line_pos(subs[sel[1]], styles, playres_x, playres_y)
        cfg.pos_x = math.floor(def_x)
        cfg.pos_y = math.floor(def_y)
    end
    
    local dropdown_dirs = {t.m_utd, t.m_dtu, t.m_ltr, t.m_rtl, t.m_c2s, t.m_s2c}

    local ventana_cascada = {
        {class = "label", x = 0, y = 0, width = 4, height = 1, label = t.c_title},
        {class = "label", x = 0, y = 1, width = 2, height = 1, label = t.c_posx},
        {class = "intedit", name = "pos_x", x = 2, y = 1, width = 2, height = 1, value = cfg.pos_x},
        {class = "label", x = 0, y = 2, width = 2, height = 1, label = t.c_posy},
        {class = "intedit", name = "pos_y", x = 2, y = 2, width = 2, height = 1, value = cfg.pos_y},
        {class = "label", x = 0, y = 3, width = 2, height = 1, label = t.c_dist},
        {class = "intedit", name = "altura", x = 2, y = 3, width = 2, height = 1, value = cfg.altura},
        {class = "label", x = 0, y = 4, width = 2, height = 1, label = t.c_vel},
        {class = "intedit", name = "velocidad", x = 2, y = 4, width = 2, height = 1, value = cfg.velocidad},
        {class = "label", x = 0, y = 5, width = 2, height = 1, label = " " .. t.mv_dir},
        {class = "dropdown", name = "c_dir_raw", x = 2, y = 5, width = 2, height = 1, items = dropdown_dirs, value = dropdown_dirs[cfg.c_dir_idx or 1]},
        {class = "label", x = 0, y = 6, width = 4, height = 1, label = t.c_footer},
        {class = "checkbox", name = "solo_primario", x = 0, y = 7, width = 4, height = 1, label = t.c_chk1, value = cfg.solo_primario},
        {class = "checkbox", name = "guardar", x = 0, y = 8, width = 4, height = 1, label = t.c_chk2, value = cfg.guardar},
        {class = "label", x = 0, y = 9, width = 4, height = 1, label = "______________________________________________________"},
    }
    
    local btn_presionado, resultados = aegisub.dialog.display(ventana_cascada, {t.c_btn_gen, t.c_btn_back}, {ok = t.c_btn_gen, cancel = t.c_btn_back})
    if btn_presionado == t.c_btn_gen then
        for k, v in ipairs(dropdown_dirs) do
            if resultados.c_dir_raw == v then resultados.c_dir_idx = k break end
        end
        resultados.c_dir_raw = nil 
        
        for k, v in pairs(cfg) do if resultados[k] == nil then resultados[k] = v end end
        guardar_config(resultados)
        local is_manual = (resultados.pos_x ~= math.floor(def_x)) or (resultados.pos_y ~= math.floor(def_y))
        procesar_cascada(subs, sel, resultados, styles, playres_x, playres_y, is_manual)
        return "Finalizado"
    else return "Volver" end
end

function menu_cascada_despedida(subs, sel, cfg, t, styles, playres_x, playres_y)
    local def_x, def_y = 640, 710
    if #sel > 0 then
        def_x, def_y, _ = get_line_pos(subs[sel[1]], styles, playres_x, playres_y)
        cfg.pos_x = math.floor(def_x)
        cfg.pos_y = math.floor(def_y)
    end
    
    local dropdown_dirs = {t.m_utd, t.m_dtu, t.m_ltr, t.m_rtl, t.m_c2s, t.m_s2c}

    local ventana_cascada_d = {
        {class = "label", x = 0, y = 0, width = 4, height = 1, label = t.cd_title},
        {class = "label", x = 0, y = 1, width = 2, height = 1, label = t.c_posx},
        {class = "intedit", name = "pos_x", x = 2, y = 1, width = 2, height = 1, value = cfg.pos_x},
        {class = "label", x = 0, y = 2, width = 2, height = 1, label = t.c_posy},
        {class = "intedit", name = "pos_y", x = 2, y = 2, width = 2, height = 1, value = cfg.pos_y},
        {class = "label", x = 0, y = 3, width = 2, height = 1, label = t.c_dist},
        {class = "intedit", name = "altura", x = 2, y = 3, width = 2, height = 1, value = cfg.altura},
        {class = "label", x = 0, y = 4, width = 2, height = 1, label = t.c_vel},
        {class = "intedit", name = "velocidad", x = 2, y = 4, width = 2, height = 1, value = cfg.velocidad},
        {class = "label", x = 0, y = 5, width = 2, height = 1, label = " " .. t.mv_dir},
        {class = "dropdown", name = "c_dir_raw", x = 2, y = 5, width = 2, height = 1, items = dropdown_dirs, value = dropdown_dirs[cfg.c_dir_idx or 1]},
        {class = "label", x = 0, y = 6, width = 4, height = 1, label = t.c_footer},
        {class = "checkbox", name = "solo_primario", x = 0, y = 7, width = 4, height = 1, label = t.c_chk1, value = cfg.solo_primario},
        {class = "checkbox", name = "guardar", x = 0, y = 8, width = 4, height = 1, label = t.c_chk2, value = cfg.guardar},
        {class = "label", x = 0, y = 9, width = 4, height = 1, label = "______________________________________________________"},
    }
    
    local btn_presionado, resultados = aegisub.dialog.display(ventana_cascada_d, {t.c_btn_gen, t.c_btn_back}, {ok = t.c_btn_gen, cancel = t.c_btn_back})
    if btn_presionado == t.c_btn_gen then
        for k, v in ipairs(dropdown_dirs) do
            if resultados.c_dir_raw == v then resultados.c_dir_idx = k break end
        end
        resultados.c_dir_raw = nil 
        
        for k, v in pairs(cfg) do if resultados[k] == nil then resultados[k] = v end end
        guardar_config(resultados)
        local is_manual = (resultados.pos_x ~= math.floor(def_x)) or (resultados.pos_y ~= math.floor(def_y))
        procesar_cascada_despedida(subs, sel, resultados, styles, playres_x, playres_y, is_manual)
        return "Finalizado"
    else return "Volver" end
end

-- ==========================================
-- SISTEMA INTEGRADO: KARAOKE BRILLANTE AVANZADO
-- ==========================================
function menu_brillante_config(subs, sel, cfg, t, styles)
    local opc_alpha = {"&H00&", "&H40&", "&H80&", "&HFF&"}
    local lista_estilos = {}
    for k, v in pairs(styles) do table.insert(lista_estilos, k) end
    if #lista_estilos == 0 then table.insert(lista_estilos, "Default") end
    table.sort(lista_estilos)

    local modo = cfg.variante or "1. Normal"
    local prefix = "n_"
    local title = t.sub_normal
    
    if modo == "2. Inverso" then 
        prefix = "i_" 
        title = t.sub_inverso
    elseif modo == "3. Desaparición" then 
        prefix = "d_" 
        title = t.sub_desap
    end

    local d = {
        {class = "label", x = 0, y = 0, width = 4, height = 1, label = title},
        {class = "label", x = 0, y = 1, width = 4, height = 1, label = "┌──────────────────────────────────────────────┐"},
        {class = "checkbox", name = prefix .. "b_auto", x = 0, y = 2, width = 4, height = 1, label = t.b_auto, value = cfg[prefix .. "b_auto"]},
        {class = "label", x = 0, y = 3, width = 1, height = 1, label = t.b_glow},
        {class = "color", name = prefix .. "col_glow", x = 1, y = 3, width = 1, height = 1, value = cfg[prefix .. "col_glow"]},
        
        {class = "label", x = 0, y = 4, width = 1, height = 1, label = t.b_c1},
        {class = "color", name = prefix .. "col_1", x = 1, y = 4, width = 1, height = 1, value = cfg[prefix .. "col_1"]},
        {class = "label", x = 2, y = 4, width = 1, height = 1, label = t.b_alp},
        {class = "dropdown", name = prefix .. "alp_1", x = 3, y = 4, width = 1, height = 1, items = opc_alpha, value = cfg[prefix .. "alp_1"]},

        {class = "label", x = 0, y = 5, width = 1, height = 1, label = t.b_c2},
        {class = "color", name = prefix .. "col_2", x = 1, y = 5, width = 1, height = 1, value = cfg[prefix .. "col_2"]},
        {class = "label", x = 2, y = 5, width = 1, height = 1, label = t.b_alp},
        {class = "dropdown", name = prefix .. "alp_2", x = 3, y = 5, width = 1, height = 1, items = opc_alpha, value = cfg[prefix .. "alp_2"]},

        {class = "label", x = 0, y = 6, width = 1, height = 1, label = t.b_c3},
        {class = "color", name = prefix .. "col_3", x = 1, y = 6, width = 1, height = 1, value = cfg[prefix .. "col_3"]},
        {class = "label", x = 2, y = 6, width = 1, height = 1, label = t.b_alp},
        {class = "dropdown", name = prefix .. "alp_3", x = 3, y = 6, width = 1, height = 1, items = opc_alpha, value = cfg[prefix .. "alp_3"]},

        {class = "label", x = 0, y = 7, width = 1, height = 1, label = t.b_c4},
        {class = "color", name = prefix .. "col_4", x = 1, y = 7, width = 1, height = 1, value = cfg[prefix .. "col_4"]},
        {class = "label", x = 2, y = 7, width = 1, height = 1, label = t.b_alp},
        {class = "dropdown", name = prefix .. "alp_4", x = 3, y = 7, width = 1, height = 1, items = opc_alpha, value = cfg[prefix .. "alp_4"]},
    }

    local layout_y = 8
    if modo == "3. Desaparición" then
        local current_style_out = cfg.d_estilo_out
        if not styles[current_style_out] then current_style_out = lista_estilos[1] end
        table.insert(d, {class = "label", x = 0, y = layout_y, width = 2, height = 1, label = t.b_style_out})
        table.insert(d, {class = "dropdown", name = "d_estilo_out", x = 2, y = layout_y, width = 2, height = 1, items = lista_estilos, value = current_style_out})
        layout_y = layout_y + 1
    end

    table.insert(d, {class = "label", x = 0, y = layout_y, width = 4, height = 1, label = "└─────────────────────────────────────────────┘"})
    
    local btn_presionado, resultados = aegisub.dialog.display(d, {"✔ Aplicar", "⬅ Volver"}, {ok = "✔ Aplicar", cancel = "⬅ Volver"})
    
    if btn_presionado == "✔ Aplicar" then
        for k, v in pairs(resultados) do cfg[k] = v end
        guardar_config(cfg)
        procesar_brillante(subs, sel, cfg, styles)
        return "Finalizado"
    else 
        return "Volver" 
    end
end

function menu_brillante(subs, sel, cfg, t, styles)
    local opc_variantes = {"1. Normal", "2. Inverso", "3. Desaparición"}
    
    while true do
        local d = {
            {class = "label", x = 0, y = 0, width = 3, height = 1, label = "◈ SELECCIÓN DE VARIANTE ◈"},
            {class = "label", x = 0, y = 1, width = 3, height = 1, label = "┌──────────────────────────────────────────────┐"},
            {class = "label", x = 0, y = 2, width = 1, height = 1, label = t.m_var},
            {class = "dropdown", name = "variante", x = 1, y = 2, width = 2, height = 1, items = opc_variantes, value = cfg.variante or "1. Normal"},
            {class = "label", x = 0, y = 3, width = 3, height = 1, label = "└──────────────────────────────────────────────┘"}
        }
        
        local btn_presionado, resultados = aegisub.dialog.display(d, {"Configurar ➔", t.c_btn_back}, {ok = "Configurar ➔", cancel = t.c_btn_back})
        
        if btn_presionado == "Configurar ➔" then
            cfg.variante = resultados.variante
            guardar_config(cfg)
            local status = menu_brillante_config(subs, sel, cfg, t, styles)
            if status == "Finalizado" then
                return "Finalizado"
            end
            -- Si cancelas en Configurar, la pantalla volverá a este menú de Selección de Variante.
        else
            return "Volver"
        end
    end
end
-- ==========================================

function menu_reverso(subs, sel, cfg, t, styles)
    local opc_alpha = {"&H00&", "&H40&", "&H80&", "&HFF&"}
    local ventana_reverso = {
        {class = "label", x = 0, y = 0, width = 4, height = 1, label = t.r_title},
        {class = "label", x = 0, y = 1, width = 4, height = 1, label = t.b_line1},
        
        {class = "checkbox", name = "r_auto", x = 0, y = 2, width = 4, height = 1, label = t.r_auto, value = cfg.r_auto},
        
        {class = "label", x = 0, y = 3, width = 1, height = 1, label = t.b_c2},
        {class = "color", name = "rev_col_2", x = 1, y = 3, width = 1, height = 1, value = cfg.rev_col_2},
        {class = "label", x = 2, y = 3, width = 1, height = 1, label = t.b_alp},
        {class = "dropdown", name = "rev_alp_2", x = 3, y = 3, width = 1, height = 1, items = opc_alpha, value = cfg.rev_alp_2},
        
        {class = "label", x = 0, y = 4, width = 1, height = 1, label = t.b_c3},
        {class = "color", name = "rev_col_3", x = 1, y = 4, width = 1, height = 1, value = cfg.rev_col_3},
        {class = "label", x = 2, y = 4, width = 1, height = 1, label = t.b_alp},
        {class = "dropdown", name = "rev_alp_3", x = 3, y = 4, width = 1, height = 1, items = opc_alpha, value = cfg.rev_alp_3},
        
        {class = "label", x = 0, y = 5, width = 1, height = 1, label = t.b_c4},
        {class = "color", name = "rev_col_4", x = 1, y = 5, width = 1, height = 1, value = cfg.rev_col_4},
        {class = "label", x = 2, y = 5, width = 1, height = 1, label = t.b_alp},
        {class = "dropdown", name = "rev_alp_4", x = 3, y = 5, width = 1, height = 1, items = opc_alpha, value = cfg.rev_alp_4},
        
        {class = "label", x = 0, y = 6, width = 4, height = 1, label = t.c_footer},
        
        {class = "checkbox", name = "rev_ytkt", x = 0, y = 7, width = 4, height = 1, label = t.r_ytkt, value = cfg.rev_ytkt},
        {class = "checkbox", name = "rev_aegi", x = 0, y = 8, width = 4, height = 1, label = t.r_aegi, value = cfg.rev_aegi},
        {class = "checkbox", name = "guardar", x = 0, y = 9, width = 4, height = 1, label = t.c_chk2, value = cfg.guardar},
        {class = "label", x = 0, y = 10, width = 4, height = 1, label = "__________________________________________________________"},
    }
    
    local btn_presionado, resultados = aegisub.dialog.display(ventana_reverso, {t.r_btn_gen, t.c_btn_back}, {ok = t.r_btn_gen, cancel = t.c_btn_back})
    if btn_presionado == t.r_btn_gen then
        for k, v in pairs(cfg) do if resultados[k] == nil then resultados[k] = v end end
        guardar_config(resultados)
        procesar_reverso(subs, sel, resultados, styles)
        return "Finalizado"
    else return "Volver" end
end

function menu_fadeworks(subs, sel, cfg, t)
    local dropdown_modos = {t.m_ltr, t.m_rtl, t.m_c2s, t.m_s2c}
    local dropdown_letras = {"40","80","120","160","200","250","300","350","400","450","500","750","1000","1250","1500"}
    local ventana_fadeworks = {
        {class = "label", x = 0, y = 0, width = 4, height = 1, label = t.f_title},
        {class = "label", x = 0, y = 1, width = 4, height = 1, label = t.b_line1},
        {class = "label", x = 0, y = 2, width = 1, height = 1, label = t.f_in},
        {class = "intedit", name = "fw_in", x = 1, y = 2, width = 1, height = 1, value = cfg.fw_in},
        {class = "label", x = 2, y = 2, width = 1, height = 1, label = t.f_out},
        {class = "intedit", name = "fw_out", x = 3, y = 2, width = 1, height = 1, value = cfg.fw_out},
        {class = "label", x = 0, y = 3, width = 1, height = 1, label = t.f_ltr},
        {class = "dropdown", name = "fw_ltr", x = 1, y = 3, width = 1, height = 1, items = dropdown_letras, value = cfg.fw_ltr},
        {class = "label", x = 2, y = 3, width = 1, height = 1, label = t.f_mode},
        {class = "dropdown", name = "fw_mode_raw", x = 3, y = 3, width = 1, height = 1, items = dropdown_modos, value = dropdown_modos[cfg.fw_mode_idx or 1]},
        {class = "label", x = 0, y = 4, width = 4, height = 1, label = t.c_footer},
        {class = "checkbox", name = "fw_prim", x = 0, y = 5, width = 4, height = 1, label = t.f_chk_prim, value = cfg.fw_prim},
        {class = "checkbox", name = "guardar", x = 0, y = 6, width = 4, height = 1, label = t.c_chk2, value = cfg.guardar},
        {class = "label", x = 0, y = 7, width = 4, height = 1, label = "__________________________________________________________"},
    }
    
    local botones = {t.f_btn_letra, t.f_btn_gen, t.c_btn_back}
    local btn_presionado, resultados = aegisub.dialog.display(ventana_fadeworks, botones, {ok = t.f_btn_letra, cancel = t.c_btn_back})
    if btn_presionado == t.f_btn_gen or btn_presionado == t.f_btn_letra then
        for k, v in ipairs(dropdown_modos) do
            if resultados.fw_mode_raw == v then resultados.fw_mode_idx = k break end
        end
        resultados.fw_mode_raw = nil 
        for k, v in pairs(cfg) do if resultados[k] == nil then resultados[k] = v end end
        guardar_config(resultados)
        
        if btn_presionado == t.f_btn_letra then procesar_fadeworks_letra(subs, sel, resultados)
        else procesar_fadeworks_linea(subs, sel, resultados) end
        return "Finalizado"
    else return "Volver" end
end

function menu_ytchroma(subs, sel, cfg, t, styles, playres_x, playres_y)
    local opc_alpha = {"&H00&", "&H40&", "&H80&", "&H85&", "&H9A&", "&HA0&", "&HBF&", "&HDB&", "&HE3&", "&HFF&"}
    
    local ventana_ytchroma = {
        {class = "label", x = 0, y = 0, width = 6, height = 1, label = t.yc_title},
        {class = "label", x = 0, y = 1, width = 6, height = 1, label = "═════════════════════════════════════════════════════════"},
        
        {class = "label", x = 0, y = 2, width = 6, height = 1, label = t.yc_sec1},
        
        {class = "label", x = 0, y = 3, width = 1, height = 1, label = t.yc_tin},
        {class = "intedit", name = "yc_tin", x = 1, y = 3, width = 2, height = 1, value = cfg.yc_tin},
        
        {class = "label", x = 3, y = 3, width = 1, height = 1, label = t.yc_tout},
        {class = "intedit", name = "yc_tout", x = 4, y = 3, width = 2, height = 1, value = cfg.yc_tout},
        
        {class = "label", x = 0, y = 4, width = 1, height = 1, label = t.yc_ox},
        {class = "floatedit", name = "yc_ox", x = 1, y = 4, width = 2, height = 1, value = cfg.yc_ox},
        
        {class = "label", x = 3, y = 4, width = 1, height = 1, label = t.yc_oy},
        {class = "floatedit", name = "yc_oy", x = 4, y = 4, width = 2, height = 1, value = cfg.yc_oy},
        
        {class = "label", x = 0, y = 5, width = 6, height = 1, label = "├────────────────────────────────────────────────────────┤"},
        
        {class = "label", x = 0, y = 6, width = 6, height = 1, label = t.yc_sec2},
        
        {class = "checkbox", name = "yc_auto", x = 0, y = 7, width = 2, height = 1, label = t.yc_auto, value = cfg.yc_auto},
        {class = "checkbox", name = "yc_glow", x = 2, y = 7, width = 2, height = 1, label = t.yc_glow, value = cfg.yc_glow},
        {class = "checkbox", name = "yc_chaos", x = 4, y = 7, width = 2, height = 1, label = t.yc_chaos, value = cfg.yc_chaos},
        
        {class = "label", x = 0, y = 8, width = 1, height = 1, label = t.yc_c3},
        {class = "color", name = "yc_c3", x=1, y=8, width=1, height=1, value=cfg.yc_c3},
        
        {class="label", x=2, y=8, width=1, height=1, label=t.yc_c2},
        {class="color", name="yc_c2", x=3, y=8, width=1, height=1, value=cfg.yc_c2},
        
        {class="label", x=4, y=8, width=1, height=1, label=t.yc_c1},
        {class="color", name="yc_c1", x=5, y=8, width=1, height=1, value=cfg.yc_c1},
        
        {class = "label", x = 0, y = 9, width = 1, height = 1, label = t.yc_a3},
        {class = "dropdown", name = "yc_a3", x = 1, y = 9, width = 1, height = 1, items = opc_alpha, value = cfg.yc_a3},
        
        {class = "label", x = 2, y = 9, width = 1, height = 1, label = t.yc_a2},
        {class = "dropdown", name = "yc_a2", x = 3, y = 9, width = 1, height = 1, items = opc_alpha, value = cfg.yc_a2},
        
        {class = "label", x = 4, y = 9, width = 1, height = 1, label = t.yc_a1},
        {class = "dropdown", name = "yc_a1", x = 5, y = 9, width = 1, height = 1, items = opc_alpha, value = cfg.yc_a1},
        
        {class = "checkbox", name = "yc_yt", x = 0, y = 10, width = 6, height = 1, label = t.yc_yt, value = cfg.yc_yt},
        
        {class = "label", x = 0, y = 11, width = 6, height = 1, label = "═════════════════════════════════════════════════════════"},
        {class = "checkbox", name = "guardar", x = 0, y = 12, width = 6, height = 1, label = t.yc_save, value = cfg.guardar},
    }
    
    local btn_presionado, resultados = aegisub.dialog.display(ventana_ytchroma, {t.yc_btn_gen, t.c_btn_back}, {ok = t.yc_btn_gen, cancel = t.c_btn_back})
    if btn_presionado == t.yc_btn_gen then
        for k, v in pairs(cfg) do if resultados[k] == nil then resultados[k] = v end end
        guardar_config(resultados)
        procesar_ytchroma(subs, sel, resultados, styles, playres_x, playres_y)
        return "Finalizado"
    else return "Volver" end
end

function menu_gradient(subs, sel, cfg, t)
    local ventana_gradient = {
        {class="label", label=t.gr_title, x=0, y=0, width=6},
        {class="label", label="┌─────────────────────────────────────────────────────────┐", x=0, y=1, width=6},
        
        {class="label", label=t.gr_app, x=0, y=2, width=1},
        {class="checkbox", name="gr_app_c", label="\\c", value=cfg.gr_app_c, x=1, y=2, width=1},
        {class="checkbox", name="gr_app_3c", label="\\3c", value=cfg.gr_app_3c, x=2, y=2, width=1},
        {class="checkbox", name="gr_app_4c", label="\\4c", value=cfg.gr_app_4c, x=3, y=2, width=1},
        {class="checkbox", name="gr_hsl", label=t.gr_hsl, value=cfg.gr_hsl, x=4, y=2, width=2},
        
        {class="label", label="├─────────────────────────────────────────────────────────┤", x=0, y=3, width=6},
        
        {class="label", label=t.gr_start, x=1, y=4, width=1},
        {class="label", label=t.gr_end, x=2, y=4, width=1},
        {class="checkbox", name="gr_u2", label="Extra 1", value=cfg.gr_u2, x=3, y=4, width=1},
        {class="checkbox", name="gr_u3", label="Extra 2", value=cfg.gr_u3, x=4, y=4, width=1},
        {class="checkbox", name="gr_u4", label="Extra 3", value=cfg.gr_u4, x=5, y=4, width=1},
        
        {class="label", label=t.gr_prim, x=0, y=5, width=1},
        {class="color", name="gr_c1", value=cfg.gr_c1, x=1, y=5, width=1},
        {class="color", name="gr_c5", value=cfg.gr_c5, x=2, y=5, width=1},
        {class="color", name="gr_c2", value=cfg.gr_c2, x=3, y=5, width=1},
        {class="color", name="gr_c3", value=cfg.gr_c3, x=4, y=5, width=1},
        {class="color", name="gr_c4", value=cfg.gr_c4, x=5, y=5, width=1},
        
        {class="label", label=t.gr_bord, x=0, y=6, width=1},
        {class="color", name="gr_b1", value=cfg.gr_b1, x=1, y=6, width=1},
        {class="color", name="gr_b5", value=cfg.gr_b5, x=2, y=6, width=1},
        {class="color", name="gr_b2", value=cfg.gr_b2, x=3, y=6, width=1},
        {class="color", name="gr_b3", value=cfg.gr_b3, x=4, y=6, width=1},
        {class="color", name="gr_b4", value=cfg.gr_b4, x=5, y=6, width=1},
        
        {class="label", label=t.gr_shad, x=0, y=7, width=1},
        {class="color", name="gr_s1", value=cfg.gr_s1, x=1, y=7, width=1},
        {class="color", name="gr_s5", value=cfg.gr_s5, x=2, y=7, width=1},
        {class="color", name="gr_s2", value=cfg.gr_s2, x=3, y=7, width=1},
        {class="color", name="gr_s3", value=cfg.gr_s3, x=4, y=7, width=1},
        {class="color", name="gr_s4", value=cfg.gr_s4, x=5, y=7, width=1},
        
        {class="label", label="├─────────────────────────────────────────────────────────┤", x=0, y=8, width=6},
        
        {class="label", label=t.gr_rgb_t, x=0, y=9, width=2},
        {class="checkbox", name="gr_rgb_on", label=t.gr_rgb_on, value=cfg.gr_rgb_on, x=2, y=9, width=1},
        {class="label", label=t.gr_cyc, x=3, y=9, width=1}, 
        {class="floatedit", name="gr_speed", value=cfg.gr_speed, x=4, y=9, width=1},
        {class="label", label=t.gr_thk, x=0, y=10, width=1}, 
        {class="floatedit", name="gr_sz", value=cfg.gr_sz, x=1, y=10, width=1},
        
        {class="label", label="└─────────────────────────────────────────────────────────┘", x=0, y=11, width=6},
        {class="checkbox", name="guardar", label=t.c_chk2, value=cfg.guardar, x=0, y=12, width=6},
        {class="label", label="___________________________________________________________________", x=0, y=13, width=6},
    }
    
    local btn_presionado, resultados = aegisub.dialog.display(ventana_gradient, {t.gr_btn, t.c_btn_back}, {ok = t.gr_btn, cancel = t.c_btn_back})
    if btn_presionado == t.gr_btn then
        for k, v in pairs(cfg) do if resultados[k] == nil then resultados[k] = v end end
        guardar_config(resultados)
        procesar_gradiente(subs, sel, resultados)
        return "Finalizado"
    else return "Volver" end
end

function menu_rgb_plus(subs, sel, cfg, t)
    local ventana_rgb_plus = {
        {class="label", label=t.rp_title, x=0, y=0, width=6},
        {class="label", label="┌─────────────────────────────────────────────────────────┐", x=0, y=1, width=6},
        
        {class="label", label=t.rp_int, x=0, y=2, width=1},
        {class="intedit", name="rf_fatias", value=cfg.rf_fatias, min=1, x=1, y=2, width=1},
        {class="label", label=t.rp_spd, x=2, y=2, width=1},
        {class="intedit", name="rf_onda", value=cfg.rf_onda, x=3, y=2, width=1},
        {class="label", label=t.rp_glow, x=4, y=2, width=1},
        {class="floatedit", name="rp_glow", value=cfg.rp_glow, min=0, max=1, step=0.1, x=5, y=2, width=1},
        
        {class="label", label=t.rp_tgt, x=0, y=3, width=2},
        {class="checkbox", name="rf_c1", label="\\c", value=cfg.rf_c1, x=2, y=3, width=1},
        {class="checkbox", name="rf_c3", label="\\3c", value=cfg.rf_c3, x=3, y=3, width=1},
        {class="checkbox", name="rf_c4", label="\\4c", value=cfg.rf_c4, x=4, y=3, width=1},
        
        {class="label", label="├─────────────────────────────────────────────────────────┤", x=0, y=4, width=6},
        {class="checkbox", name="rf_man", label=t.rp_man, value=cfg.rf_man, x=0, y=5, width=6},
        
        {class="label", label=" Color 1:", x=0, y=6, width=1}, {class="color", name="rf_k1", value=cfg.rf_k1, x=1, y=6, width=1}, 
        {class="label", label=" Color 2:", x=2, y=6, width=1}, {class="color", name="rf_k2", value=cfg.rf_k2, x=3, y=6, width=1}, 
        {class="label", label=" Color 3:", x=4, y=6, width=1}, {class="color", name="rf_k3", value=cfg.rf_k3, x=5, y=6, width=1},
        {class="label", label=" Color 4:", x=0, y=7, width=1}, {class="color", name="rf_k4", value=cfg.rf_k4, x=1, y=7, width=1}, 
        
        {class="label", label="└─────────────────────────────────────────────────────────┘", x=0, y=8, width=6},
        {class="checkbox", name="guardar", label=t.c_chk2, value=cfg.guardar, x=0, y=9, width=6},
        {class="label", label="___________________________________________________________________", x=0, y=10, width=6},
    }
    
    local btn_presionado, resultados = aegisub.dialog.display(ventana_rgb_plus, {t.rp_btn, t.c_btn_back}, {ok = t.rp_btn, cancel = t.c_btn_back})
    if btn_presionado == t.rp_btn then
        for k, v in pairs(cfg) do if resultados[k] == nil then resultados[k] = v end end
        guardar_config(resultados)
        procesar_rgb_plus(subs, sel, resultados)
        return "Finalizado"
    else return "Volver" end
end

function menu_recolocator(subs, sel, cfg, t, styles, playres_x, playres_y)
    local dropdown_dirs = {t.m_ltr, t.m_rtl, t.m_utd, t.m_dtu}
    
    local def_x, def_y = 640, 710
    if #sel > 0 then
        def_x, def_y, _ = get_line_pos(subs[sel[1]], styles, playres_x, playres_y)
        cfg.mv_pos_x = math.floor(def_x)
        cfg.mv_pos_y = math.floor(def_y)
    end

    local ventana_move = {
        {class = "label", x = 0, y = 0, width = 6, height = 1, label = t.mv_title},
        {class = "label", x = 0, y = 1, width = 6, height = 1, label = "┌─────────────────────────────────────────────────────────┐"},
        
        {class = "label", x = 0, y = 2, width = 2, height = 1, label = t.c_posx},
        {class = "intedit", name = "mv_pos_x", x = 2, y = 2, width = 1, height = 1, value = cfg.mv_pos_x},
        
        {class = "label", x = 3, y = 2, width = 2, height = 1, label = t.c_posy},
        {class = "intedit", name = "mv_pos_y", x = 5, y = 2, width = 1, height = 1, value = cfg.mv_pos_y},
        
        {class = "label", x = 0, y = 3, width = 2, height = 1, label = t.mv_dist},
        {class = "intedit", name = "mv_dist", x = 2, y = 3, width = 1, height = 1, value = cfg.mv_dist},
        
        {class = "label", x = 3, y = 3, width = 2, height = 1, label = t.mv_dur},
        {class = "intedit", name = "mv_dur", x = 5, y = 3, width = 1, height = 1, value = cfg.mv_dur},
        
        {class = "label", x = 0, y = 4, width = 2, height = 1, label = t.mv_dir},
        {class = "dropdown", name = "mv_dir_raw", x = 2, y = 4, width = 2, height = 1, items = dropdown_dirs, value = dropdown_dirs[cfg.mv_dir_idx or 1]},
        
        {class = "checkbox", name = "mv_out", x = 4, y = 4, width = 2, height = 1, label = t.mv_out, value = cfg.mv_out},
        
        {class = "label", x = 0, y = 5, width = 6, height = 1, label = "└─────────────────────────────────────────────────────────┘"},
        {class = "checkbox", name = "guardar", x = 0, y = 6, width = 6, height = 1, label = t.c_chk2, value = cfg.guardar},
        {class = "label", x = 0, y = 7, width = 6, height = 1, label = "___________________________________________________________________"},
    }

    local btn_presionado, resultados = aegisub.dialog.display(ventana_move, {t.mv_btn, t.c_btn_back}, {ok = t.mv_btn, cancel = t.c_btn_back})
    if btn_presionado == t.mv_btn then
        for k, v in ipairs(dropdown_dirs) do
            if resultados.mv_dir_raw == v then resultados.mv_dir_idx = k break end
        end
        resultados.mv_dir_raw = nil 
        for k, v in pairs(cfg) do if resultados[k] == nil then resultados[k] = v end end
        guardar_config(resultados)
        local is_manual = (resultados.mv_pos_x ~= math.floor(def_x)) or (resultados.mv_pos_y ~= math.floor(def_y))
        procesar_recolocator(subs, sel, resultados, styles, playres_x, playres_y, is_manual)
        return "Finalizado"
    else return "Volver" end
end

-- ==========================================
-- Función Principal del GUI
-- ==========================================
function abrir_menu(subs, sel, act)
    if not verificar_acceso_patreon() then
        return 
    end

    local indice_funcion = 1 
    local modo_info = "gen" 

    local styles = {}
    local playres_x, playres_y = 1280, 720
    for i = 1, #subs do
        local l = subs[i]
        if l.class == "info" then
            if l.key == "PlayResX" then playres_x = tonumber(l.value) or playres_x end
            if l.key == "PlayResY" then playres_y = tonumber(l.value) or playres_y end
        elseif l.class == "style" then
            styles[l.name] = l
        elseif l.class == "dialogue" then break end
    end

    while true do
        local cfg = cargar_config()
        local lang = cfg.lang or "es"
        local t = loc[lang]
        
        local func_items = {t.opt_1, t.opt_2, t.opt_3, t.opt_4, t.opt_5, t.opt_6, t.opt_7, t.opt_8, t.opt_9}
        local funcion_val = func_items[indice_funcion]
        
        local texto_info = t.desc_gen
        if modo_info == "1" then texto_info = t.desc_1
        elseif modo_info == "2" then texto_info = t.desc_2
        elseif modo_info == "3" then texto_info = t.desc_3
        elseif modo_info == "4" then texto_info = t.desc_4
        elseif modo_info == "5" then texto_info = t.desc_5
        elseif modo_info == "6" then texto_info = t.desc_6
        elseif modo_info == "7" then texto_info = t.desc_7
        elseif modo_info == "8" then texto_info = t.desc_8
        elseif modo_info == "9" then texto_info = t.desc_9 end

        local dialog_config = {
            {class = "label", x = 0, y = 0, width = 6, height = 1, label = "┌──────────────────────────────────────────────────────────┐"},
            {class = "label", x = 0, y = 1, width = 6, height = 1, label = "                             ◈ S&SFX KARA SUITE     "},
            {class = "label", x = 0, y = 2, width = 6, height = 1, label = "└──────────────────────────────────────────────────────────┘"},
            {class = "label", x = 0, y = 3, width = 6, height = 1, label = " "},
            
            {class = "label", x = 0, y = 4, width = 2, height = 1, label = t.sel_func},
            {class = "dropdown", name = "funcion", x = 2, y = 4, width = 4, height = 1, items = func_items, value = funcion_val},
            
            {class = "label", x = 0, y = 5, width = 6, height = 1, label = t.info_script},
            {class = "textbox", name = "info", x = 0, y = 6, width = 6, height = 6, text = texto_info, readonly=true},
             
            {class = "label", x = 0, y = 12, width = 6, height = 1, label = "────────────────────────────────────────────────────────────"},
        }

        local botones = {t.btn_exec, t.btn_info, t.btn_lang, t.btn_close}
        local boton_presionado, resultados = aegisub.dialog.display(dialog_config, botones, {ok = t.btn_exec, cancel = t.btn_close})

        -- ARREGLO DE CONTROL: Si el usuario pulsa Cerrar o la "X" nativa de Windows, se rompe el bucle
        if boton_presionado == t.btn_close or not boton_presionado then
            break
        end

        if resultados then
            for k, v in ipairs(func_items) do
                if resultados.funcion == v then indice_funcion = k break end
            end
        end

        if boton_presionado == t.btn_lang then
            menu_idioma(cfg, t)
            
        elseif boton_presionado == t.btn_info then
            modo_info = tostring(indice_funcion)
            
        elseif boton_presionado == t.btn_exec then
            if indice_funcion == 1 then
                if menu_cascada(subs, sel, cfg, t, styles, playres_x, playres_y) ~= "Volver" then break end
            elseif indice_funcion == 2 then
                if menu_brillante(subs, sel, cfg, t, styles) ~= "Volver" then break end
            elseif indice_funcion == 3 then
                if menu_reverso(subs, sel, cfg, t, styles) ~= "Volver" then break end
            elseif indice_funcion == 4 then
                if menu_fadeworks(subs, sel, cfg, t) ~= "Volver" then break end
            elseif indice_funcion == 5 then
                if menu_ytchroma(subs, sel, cfg, t, styles, playres_x, playres_y) ~= "Volver" then break end
            elseif indice_funcion == 6 then
                if menu_gradient(subs, sel, cfg, t) ~= "Volver" then break end
            elseif indice_funcion == 7 then
                if menu_rgb_plus(subs, sel, cfg, t) ~= "Volver" then break end
            elseif indice_funcion == 8 then
                if menu_recolocator(subs, sel, cfg, t, styles, playres_x, playres_y) ~= "Volver" then break end
            elseif indice_funcion == 9 then
                if menu_cascada_despedida(subs, sel, cfg, t, styles, playres_x, playres_y) ~= "Volver" then break end
            end
        end
    end
end

aegisub.register_macro(script_name, script_description, abrir_menu)