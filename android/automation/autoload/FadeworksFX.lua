-- ==========================================
-- Metadatos del script
-- ==========================================
script_name = "FadeworksFX"
script_description = "Efecto FadeWorks (Glow animado que recupera los colores del estilo)"
script_author = "Soniz / Gemini"
script_version = "3.3"

local unicode = require("aegisub.unicode")

-- ==========================================
-- Diccionario de Idiomas
-- ==========================================
local loc = {
    es = {
        f_title = "                          ◈ CONFIGURACIÓN FADEWORKS ◈",
        b_line1 = "┌──────────────────────────────────────────────┐",
        f_in = " Fade in (ms):", f_out = " Fade out (ms):", f_ltr = " Por letra (ms):", f_mode = " Dirección:",
        c_footer = "└─────────────────────────────────────────────┘",
        c_chk2 = "[💾] Guardar Reciente",
        f_btn_gen = "✔ Aplicar Fade", f_btn_letra = "🔠 Por Letra", c_btn_back = "✕ Cancelar",
        m_ltr = "LTR (Izq -> Der)", m_rtl = "RTL (Der -> Izq)", m_c2s = "Centro -> Lados", m_s2c = "Lados -> Centro",
    }
}

-- ==========================================
-- Sistema de Guardado de Configuración
-- ==========================================
local config_file = aegisub.decode_path("?user/") .. "fadeworksfx_cfg.txt"

local function cargar_config()
    local cfg = {
        guardar = true, lang = "es",
        fw_in = 0, fw_out = 0, fw_ltr = "120", fw_mode_idx = 1,
        fw_glow = false, fw_glow_delay = 50, fw_glow_dur = 450,
        fw_col_ilu = "#FFFFFF", fw_col_bg = "#471C95"
    }
    local file = io.open(config_file, "r")
    if file then
        for line in file:lines() do
            local k, v = string.match(line, "([^=]+)=(.+)")
            if k and v then
                if k == "fw_ltr" or k:match("^fw_col") then cfg[k] = v
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
-- Funciones auxiliares
-- ==========================================
local function clonar_linea(linea)
    local nueva = {}
    for k, v in pairs(linea) do nueva[k] = v end
    return nueva
end

local function hex_to_ass(hex)
    if not hex then return "&HFFFFFF&" end
    local r, g, b = string.match(hex, "#(%x%x)(%x%x)(%x%x)")
    if r and g and b then return string.format("&H%s%s%s&", b, g, r) end
    return hex
end

-- Rescata tags estructurales
local function get_base_tags(text)
    local base = ""
    local an = text:match("(\\an%d)")
    if an then base = base .. an end
    local pos = text:match("(\\pos%b())")
    if pos then base = base .. pos end
    local org = text:match("(\\org%b())")
    if org then base = base .. org end
    return base
end

-- ==========================================
-- MOTOR FADEWORKS
-- ==========================================
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
        subs.insert(indice_real + offset_local, nueva_linea)
        lineas_insertadas = lineas_insertadas + 1
    end
    aegisub.set_undo_point("Efecto FadeWorks (Línea)")
end

function procesar_fadeworks_letra(subs, sel, cfg)
    local f_in = tonumber(cfg.fw_in) or 0
    local f_out = tonumber(cfg.fw_out) or 0
    local f_ltr = tonumber(cfg.fw_ltr) or 120
    local mode_idx = cfg.fw_mode_idx

    -- Capturar los colores originales de los Estilos del script
    local styles = {}
    for i = 1, #subs do
        local l = subs[i]
        if l.class == "style" then
            -- Quitar el canal alfa inicial para usar en los override tags (\c, \3c, \4c)
            styles[l.name] = {
                c1 = tostring(l.color1):gsub("^&H%x%x", "&H"),
                c3 = tostring(l.color3):gsub("^&H%x%x", "&H"),
                c4 = tostring(l.color4):gsub("^&H%x%x", "&H")
            }
        end
    end

    local lineas_insertadas = 0
    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]
        
        local base_tags = get_base_tags(line.text)
        local clean_text = line.text:gsub("{[^}]+}", "")
        
        local chars = {}
        for c in unicode.chars(clean_text) do
            table.insert(chars, c)
        end

        local num_chars = #chars
        if num_chars > 0 then
            
            -- ==========================
            -- MODO GLOW AISLADO
            -- ==========================
            if cfg.fw_glow then
                local c_ilu = hex_to_ass(cfg.fw_col_ilu)
                local c_bg = hex_to_ass(cfg.fw_col_bg)
                local delay = tonumber(cfg.fw_glow_delay) or 50
                local dur = tonumber(cfg.fw_glow_dur) or 450
                local center = (num_chars + 1) / 2

                -- Obtener los colores reales de la línea actual según su Estilo
                local s_c1 = styles[line.style] and styles[line.style].c1 or "&HFFFFFF&"
                local s_c3 = styles[line.style] and styles[line.style].c3 or "&HFFFFFF&"
                local s_c4 = styles[line.style] and styles[line.style].c4 or "&H000000&"

                -- 1. Crear línea base oscura (El selector de color ahora solo afecta a \3c)
                local bg_line = clonar_linea(line)
                bg_line.text = string.format("{%s\\c&H000000&\\3c%s\\4c&H000000&}%s", base_tags, c_bg, clean_text)
                bg_line.effect = "[ DARK ]"
                
                local nuevas_lineas = { bg_line }
                local max_dist = math.floor(math.abs(1 - center))

                -- 2. Crear las capas iluminadas
                for c = 1, num_chars do
                    local hl_line = clonar_linea(line)
                    hl_line.effect = ""
                    
                    local multiplier = 0
                    if mode_idx == 1 then multiplier = c - 1
                    elseif mode_idx == 2 then multiplier = num_chars - c
                    elseif mode_idx == 3 then multiplier = math.floor(math.abs(c - center))
                    elseif mode_idx == 4 then multiplier = max_dist - math.floor(math.abs(c - center))
                    end
                    
                    hl_line.start_time = line.start_time + (multiplier * delay)
                    
                    local text = string.format("{%s", base_tags)
                    
                    -- ¡Magia aquí! Transiciona de tu "Color de iluminación" a los colores reales de tu Estilo
                    local tag_glow = string.format("\\alpha&H00&\\c%s\\3c%s\\4c%s\\t(0,%d,1,\\c%s\\3c%s\\4c%s)", c_ilu, c_ilu, c_ilu, dur, s_c1, s_c3, s_c4)
                    
                    if c > 1 then
                        text = text .. "\\alpha&HFF&}"
                        for j = 1, c - 1 do text = text .. chars[j] end
                        text = text .. "{" .. tag_glow .. "}" .. chars[c]
                    else
                        text = text .. tag_glow .. "}" .. chars[c]
                    end
                    
                    if c < num_chars then
                        text = text .. "{\\alpha&HFF&}"
                        for j = c + 1, num_chars do text = text .. chars[j] end
                    end
                    
                    hl_line.text = text
                    table.insert(nuevas_lineas, hl_line)
                end

                -- Reemplazar e insertar limpiamente en orden
                line.comment = true
                subs[indice_real] = line
                
                for j = 1, #nuevas_lineas do
                    subs.insert(indice_real + j, nuevas_lineas[j])
                end
                
                lineas_insertadas = lineas_insertadas + #nuevas_lineas

            -- ==========================
            -- MODO FADE NORMAL
            -- ==========================
            else
                local out_text = ""
                local factor_center = (num_chars - 1) / 2
                
                for idx = 0, num_chars - 1 do
                    local char = chars[idx + 1]
                    local factor = 0
                    if mode_idx == 1 then factor = idx / math.max(1, num_chars - 1)
                    elseif mode_idx == 2 then factor = 1 - (idx / math.max(1, num_chars - 1))
                    elseif mode_idx == 3 then factor = math.abs(idx - factor_center) / math.max(1, factor_center)
                    elseif mode_idx == 4 then factor = 1 - (math.abs(idx - factor_center) / math.max(1, factor_center)) end

                    local tags = ""
                    local line_dur = line.end_time - line.start_time
                    
                    if f_in > 0 then
                        local spread_in = math.max(0, f_in - f_ltr)
                        local start_in = math.floor(factor * spread_in)
                        local end_in = start_in + f_ltr
                        tags = tags .. string.format("\\t(%d,%d,1,\\alpha&H00&)", start_in, end_in)
                    end
                    if f_out > 0 then
                        local spread_out = math.max(0, f_out - f_ltr)
                        local start_out = line_dur - f_out + math.floor(factor * spread_out)
                        local end_out = start_out + f_ltr
                        tags = tags .. string.format("\\t(%d,%d,1,\\alpha&HFF&)", start_out, end_out)
                    end

                    if tags ~= "" then
                        local prefix = (f_in > 0) and ("\\alpha&HFF&") or ""
                        out_text = out_text .. string.format("{%s%s}%s", prefix, tags, char)
                    else out_text = out_text .. char end
                end

                if out_text:match("^{") then
                    out_text = out_text:gsub("^{", "{" .. base_tags, 1)
                else
                    out_text = "{" .. base_tags .. "}" .. out_text
                end

                local nueva_linea = clonar_linea(line)
                nueva_linea.text = out_text
                nueva_linea.effect = "Fadeworks Normal"

                line.comment = true
                subs[indice_real] = line
                subs.insert(indice_real + 1, nueva_linea)
                lineas_insertadas = lineas_insertadas + 1
            end
        end
    end
    aegisub.set_undo_point("Efecto FadeWorks")
end

-- ==========================================
-- Menú de configuración FadeWorks
-- ==========================================
function menu_fadeworks(subs, sel, cfg, t)
    local dropdown_modos = {t.m_ltr, t.m_rtl, t.m_c2s, t.m_s2c}
    local dropdown_letras = {"40","50","80","120","160","200","250","300","350","400","450","500","750","1000","1250","1500"}
    local ventana_fadeworks = {
        {class = "label", x = 0, y = 0, width = 6, height = 1, label = t.f_title},
        {class = "label", x = 0, y = 1, width = 6, height = 1, label = t.b_line1},
        
        {class = "label", x = 0, y = 2, width = 1, height = 1, label = t.f_in},
        {class = "intedit", name = "fw_in", x = 1, y = 2, width = 1, height = 1, value = cfg.fw_in},
        {class = "label", x = 2, y = 2, width = 1, height = 1, label = t.f_out},
        {class = "intedit", name = "fw_out", x = 3, y = 2, width = 1, height = 1, value = cfg.fw_out},
        
        {class = "label", x = 0, y = 3, width = 1, height = 1, label = t.f_ltr},
        {class = "dropdown", name = "fw_ltr", x = 1, y = 3, width = 1, height = 1, items = dropdown_letras, value = cfg.fw_ltr},
        {class = "label", x = 2, y = 3, width = 1, height = 1, label = t.f_mode},
        {class = "dropdown", name = "fw_mode_raw", x = 3, y = 3, width = 3, height = 1, items = dropdown_modos, value = dropdown_modos[cfg.fw_mode_idx or 1]},
        
        -- Sección GLOW
        {class = "checkbox", name = "fw_glow", x = 0, y = 4, width = 2, height = 1, label = "✨ Activar Glow", value = cfg.fw_glow},
        {class = "label", x = 2, y = 4, width = 1, height = 1, label = " Retraso (ms):"},
        {class = "intedit", name = "fw_glow_delay", x = 3, y = 4, width = 1, height = 1, value = cfg.fw_glow_delay},
        {class = "label", x = 4, y = 4, width = 1, height = 1, label = " Luz dur (ms):"},
        {class = "intedit", name = "fw_glow_dur", x = 5, y = 4, width = 1, height = 1, value = cfg.fw_glow_dur},
        
        {class = "label", x = 0, y = 5, width = 1, height = 1, label = " Iluminación:"},
        {class = "color", name = "fw_col_ilu", x = 1, y = 5, width = 2, height = 1, value = cfg.fw_col_ilu},
        {class = "label", x = 3, y = 5, width = 1, height = 1, label = " Borde Fondo:"},
        {class = "color", name = "fw_col_bg", x = 4, y = 5, width = 2, height = 1, value = cfg.fw_col_bg},
        
        {class = "label", x = 0, y = 6, width = 6, height = 1, label = t.c_footer},
        {class = "checkbox", name = "guardar", x = 0, y = 7, width = 6, height = 1, label = t.c_chk2, value = cfg.guardar},
        {class = "label", x = 0, y = 8, width = 6, height = 1, label = "__________________________________________________________"},
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

-- ==========================================
-- Punto de entrada de la macro
-- ==========================================
function abrir_fadeworks(subs, sel)
    local cfg = cargar_config()
    local t = loc.es
    menu_fadeworks(subs, sel, cfg, t)
end

-- ==========================================
-- Registro de la Macro
-- ==========================================
aegisub.register_macro("FadeworksFX/Aplicar FadeWorks", "Aplica el efecto FadeWorks (fad general o glow por letra)", abrir_fadeworks)