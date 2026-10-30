script_name = "GlitchFX"
script_description = "Chroma - Aberración chromatica"
script_author = "GH2382 / SonizzIdk"
script_version = "2.0"

local util = require("aegisub.util")

-- Valores por defecto (ya no se editan desde el diálogo)
local FRAME_DURATION = 70
local ALFA = {
    a = { ini = 80, fin = 135 },
    b = { ini = 80, fin = 136 },
    c = { ini = 80, fin = 129 },
}

-- Parámetros del motor "Aberración" (estilo Chroma Soniz / YTSubConverter)
local ABER = {
    step = 33,                       -- ~30 fps
    overlap = 66,                    -- ~2 frames de solape
    ox = 18.4,                       -- desfase X
    oy = 0,                          -- desfase Y
    alfa = { "DB", "A0", "85" },     -- alpha de las capas A, B, C
}

local function hex_a_ass(hex_color)
    local r, g, b = hex_color:match("#(%x%x)(%x%x)(%x%x)")
    if not r then return "&HFFFFFF&" end
    return string.format("&H%s%s%s&", b, g, r)
end

local function int_a_hex(val)
    return string.format("%02X", math.min(255, math.max(0, tonumber(val) or 0)))
end

local function interpolar_color(color_ini_hex, color_fin_hex, progreso)
    local c1 = color_ini_hex:gsub("&H", ""):gsub("&", "")
    local c2 = color_fin_hex:gsub("&H", ""):gsub("&", "")

    local b1 = tonumber(c1:sub(1, 2), 16) or 0
    local g1 = tonumber(c1:sub(3, 4), 16) or 0
    local r1 = tonumber(c1:sub(5, 6), 16) or 0

    local b2 = tonumber(c2:sub(1, 2), 16) or 0
    local g2 = tonumber(c2:sub(3, 4), 16) or 0
    local r2 = tonumber(c2:sub(5, 6), 16) or 0

    local b = math.floor(b1 + (b2 - b1) * progreso)
    local g = math.floor(g1 + (g2 - g1) * progreso)
    local r = math.floor(r1 + (r2 - r1) * progreso)

    return string.format("&H%02X%02X%02X&", b, g, r)
end

local function interpolar_alfa(alfa_ini_hex, alfa_fin_hex, progreso)
    local a1 = tonumber(alfa_ini_hex:gsub("&H", ""):gsub("&", ""), 16) or 0
    local a2 = tonumber(alfa_fin_hex:gsub("&H", ""):gsub("&", ""), 16) or 0
    local a_actual = math.floor(a1 + (a2 - a1) * progreso)
    return string.format("&H%02X&", a_actual)
end

-- util.copy() hace una copia superficial: sin una tabla "extra" nueva, todas las
-- líneas generadas compartirían la misma tabla por referencia y el dato de carpeta
-- (_aegi_folddata) contaminaría a las demás. Esta función devuelve una tabla propia.
local function extra_independiente(origen, excluir_clave)
    local nueva = {}
    if type(origen) == "table" then
        for k, v in pairs(origen) do
            if k ~= excluir_clave then nueva[k] = v end
        end
    end
    return nueva
end

-- Color de la línea: primero el \c / \1c inline, si no existe el color primario del estilo.
-- Devuelve el formato ASS "&HBBGGRR&".
local function color_de_linea(line, estilo)
    local hex = line.text:match("\\1?c&H(%x+)&")
    if hex then
        hex = string.rep("0", math.max(0, 6 - #hex)) .. hex
        return "&H" .. hex:sub(-6):upper() .. "&"
    end
    local c = (estilo and estilo.color1) or "&H00FFFFFF"
    local h = c:gsub("&H", ""):gsub("&", "")
    h = string.rep("0", math.max(0, 6 - #h)) .. h
    return "&H" .. h:sub(-6):upper() .. "&"
end

local function extraer_tags_originales(texto_completo)
    local tags_acumulados = ""
    for bloque in texto_completo:gmatch("{([^}]+)}") do
        local limpio = bloque:gsub("\\pos%([^)]+%)", "")
                             :gsub("\\1?c&H%x+&", "")
                             :gsub("\\1a&H%x+&", "")
                             :gsub("\\3a&H%x+&", "")
                             :gsub("\\4a&H%x+&", "")
        if limpio ~= "" then
            tags_acumulados = tags_acumulados .. limpio
        end
    end
    if tags_acumulados ~= "" and not tags_acumulados:match("^\\") then
        tags_acumulados = "\\" .. tags_acumulados
    end
    return tags_acumulados
end

function glitch_secuencial(subtitles, selected_lines, active_line)
    local dialogo_config = {
        {class="label", label="--- TIEMPOS DEL EFECTO ---", x=0, y=0, width=4},
        {class="label", label="Entrada (ms):", x=0, y=1},
        {class="intedit", name="entrada", value=300, min=0, x=1, y=1},
        {class="label", label="Salida (ms):", x=2, y=1},
        {class="intedit", name="salida", value=300, min=0, x=3, y=1},

        {class="label", label="--- CAPAS DE COLOR ---", x=0, y=2, width=4},
        {class="checkbox", name="auto_colors", label="Auto colors (tomar color del estilo / línea)", value=false, x=0, y=3, width=4},
        {class="checkbox", name="aberracion", label="Aberración", value=false, x=0, y=4, width=4,
            hint="Usa el motor de aberración cromática de Chroma Soniz"},

        {class="label", label="Capa A (+X/+Y):", x=0, y=5},
        {class="color", name="c1_color", value="#FF1313", x=1, y=5},
        {class="label", label="Capa B (Centro):", x=0, y=6},
        {class="color", name="c2_color", value="#13FF13", x=1, y=6},
        {class="label", label="Capa C (-X/-Y):", x=0, y=7},
        {class="color", name="c3_color", value="#1313FF", x=1, y=7},

        {class="label", label="Efecto de Aberración Chromatica", x=0, y=8, width=4}
    }

    local btn, res = aegisub.dialog.display(dialogo_config, {"Aplicar efecto", "Cancelar"})
    if btn ~= "Aplicar efecto" then
        aegisub.cancel()
    end

    local entrada_t = tonumber(res.entrada) or 300
    local salida_t = tonumber(res.salida) or 300
    local frame_duration = FRAME_DURATION
    local auto_colors = res.auto_colors

    local a_ini = { int_a_hex(ALFA.a.ini), int_a_hex(ALFA.b.ini), int_a_hex(ALFA.c.ini) }
    local a_fin = { int_a_hex(ALFA.a.fin), int_a_hex(ALFA.b.fin), int_a_hex(ALFA.c.fin) }

    local manual_colors = {
        hex_a_ass(res.c1_color),
        hex_a_ass(res.c2_color),
        hex_a_ass(res.c3_color)
    }

    local desv_x = 12.288
    local desv_y = 0.0

    -- Resolución del archivo
    local play_res_x, play_res_y = 1920, 1080
    for i = 1, #subtitles do
        local l = subtitles[i]
        if l.class == "info" then
            if l.key == "PlayResX" then play_res_x = tonumber(l.value) or play_res_x end
            if l.key == "PlayResY" then play_res_y = tonumber(l.value) or play_res_y end
        end
    end

    local estilos = {}
    for i = 1, #subtitles do
        local l = subtitles[i]
        if l.class == "style" then estilos[l.name] = l end
    end

    for i = #selected_lines, 1, -1 do
        local index = selected_lines[i]
        local line = subtitles[index]

        local total_start = line.start_time
        local total_end = line.end_time
        local total_duration = total_end - total_start

        if entrada_t + salida_t > total_duration then
            aegisub.debug.out("Error: Los efectos superan la duración total de la línea.\n")
            aegisub.cancel()
        end

        local estilo_actual = estilos[line.style] or { fontsize = 65, margin_l = 0, margin_r = 0, margin_t = 0, margin_b = 0, margin_v = 0, align = 2 }
        local fs_inline = line.text:match("\\fs([%d%.]+)")
        local fs_estilo = fs_inline and tonumber(fs_inline) or (estilo_actual.fontsize or 65)

        -- Colores de las 3 capas (manuales o automáticos)
        local color_capa = manual_colors
        if auto_colors then
            local c = color_de_linea(line, estilos[line.style])
            color_capa = { c, c, c }
        end

        local canales = {
            { color_ini = color_capa[1], alfa_ini = a_ini[1], color_fin = color_capa[1], alfa_fin = a_fin[1], mult_x = 1,  mult_y = 1,  capa_z = 1 },
            { color_ini = color_capa[2], alfa_ini = a_ini[2], color_fin = color_capa[2], alfa_fin = a_fin[2], mult_x = 0,  mult_y = 0,  capa_z = 3 },
            { color_ini = color_capa[3], alfa_ini = a_ini[3], color_fin = color_capa[3], alfa_fin = a_fin[3], mult_x = -1, mult_y = -1, capa_z = 2 }
        }

        -- Posicionamiento
        local centro_x, y_base
        local pos_x, pos_y = line.text:match("\\pos%s*%([%s]*([%d%.%-]+)[%s]*,[%s]*([%d%.%-]+)[%s]*%)")

        if pos_x and pos_y then
            centro_x = tonumber(pos_x)
            y_base = tonumber(pos_y)
        else
            local aliniacion_tag = line.text:match("\\an(%d)")
            local aliniacion = tonumber(aliniacion_tag) or estilo_actual.align or 2

            local m_l = (line.margin_l and line.margin_l > 0) and line.margin_l or estilo_actual.margin_l or 0
            local m_r = (line.margin_r and line.margin_r > 0) and line.margin_r or estilo_actual.margin_r or 0
            local m_t = (line.margin_t and line.margin_t > 0) and line.margin_t or (line.margin_v and line.margin_v > 0) and line.margin_v or estilo_actual.margin_t or estilo_actual.margin_v or 0
            local m_b = (line.margin_b and line.margin_b > 0) and line.margin_b or (line.margin_v and line.margin_v > 0) and line.margin_v or estilo_actual.margin_b or estilo_actual.margin_v or 0

            if aliniacion % 3 == 1 then centro_x = m_l
            elseif aliniacion % 3 == 0 then centro_x = play_res_x - m_r
            else centro_x = (play_res_x / 2) + (m_l - m_r) / 2 end

            if aliniacion <= 3 then y_base = play_res_y - m_b
            elseif aliniacion >= 7 then y_base = m_t
            else y_base = play_res_y / 2 end
        end

        local tags_originales = extraer_tags_originales(line.text)
        local texto_limpio = line.text:gsub("{[^}]+}", "")

        local tiempo_entrada_fin = total_start + entrada_t
        local tiempo_salida_ini = total_end - salida_t

        local lineas_nuevas = {}

        -- Motor "Aberración" (Chroma Soniz): frames de 33 ms con solape de 66 ms,
        -- desfase fijo, alphas fijos y parpadeo cromático cada 3er frame.
        local modo_aber = res.aberracion
        local frame_aber = 0

        local function generar_aberracion(t_ini, t_fin)
            local tags_aber = tags_originales
                :gsub("\\[1234]?c&H%x+&", "")
                :gsub("\\[1234]?a&H%x+&", "")
                :gsub("\\alpha&H%x+&", "")
                :gsub("\\blur[%d%.]+", "")

            for t = t_ini, t_fin - 1, ABER.step do
                local cur_end = math.min(t + ABER.overlap, total_end)
                local activo = (frame_aber % 3) ~= 2
                local ox = activo and ABER.ox or 0
                local oy = activo and ABER.oy or 0

                for k, canal in ipairs(canales) do
                    local l_nueva = util.copy(line)
                    l_nueva.start_time = t
                    l_nueva.end_time = cur_end
                    l_nueva.effect = "Glitch"
                    l_nueva.extra = extra_independiente(line.extra, "_aegi_folddata")

                    local x_pos = centro_x + (ox * canal.mult_x)
                    local y_pos = y_base + (oy * canal.mult_y)

                    l_nueva.text = string.format("{%s\\pos(%g,%g)\\fs%g\\b1\\3a&HFF&\\4a&HFF&\\1c%s\\1a&H%s&}%s",
                        tags_aber, x_pos, y_pos, fs_estilo, canal.color_fin, ABER.alfa[k], texto_limpio)

                    table.insert(lineas_nuevas, l_nueva)
                end

                frame_aber = frame_aber + 1
            end
        end

        -- CARPETA ARCH1T3CT (apertura, plegada)
        local f_start = util.copy(line)
        f_start.layer = 0
        f_start.comment = true
        f_start.effect = "Folder"
        f_start.text = "Chroma: " .. texto_limpio
        f_start.extra = { _aegi_folddata = "0;1;1" }
        table.insert(lineas_nuevas, f_start)

        -- RESPALDO DE LÍNEA ORIGINAL
        local orig = util.copy(line)
        orig.comment = true
        orig.effect = "Línea Original"
        orig.extra = extra_independiente(line.extra, "_aegi_folddata")
        table.insert(lineas_nuevas, orig)

        -- FASE 1: Entrada
        local current_start = total_start
        local frame_index = 0

        if modo_aber then generar_aberracion(total_start, tiempo_entrada_fin) end

        while not modo_aber and current_start < tiempo_entrada_fin do
            local current_end = current_start + frame_duration
            if current_end > tiempo_entrada_fin then current_end = tiempo_entrada_fin end

            local progreso = (entrada_t > 0) and ((current_start - total_start) / entrada_t) or 1.0
            if progreso > 1.0 then progreso = 1.0 end

            local aplicar_desvio = (frame_index ~= 3)

            for _, canal in ipairs(canales) do
                local l_nueva = util.copy(line)
                l_nueva.start_time = current_start
                l_nueva.end_time = current_end
                l_nueva.effect = "Glitch"
                l_nueva.layer = canal.capa_z
                l_nueva.extra = extra_independiente(line.extra, "_aegi_folddata")

                local x_pos = aplicar_desvio and (centro_x + (desv_x * canal.mult_x)) or centro_x
                local y_pos = aplicar_desvio and (y_base + (desv_y * canal.mult_y)) or y_base

                local color_actual = interpolar_color(canal.color_ini, canal.color_fin, progreso)
                local alfa_actual = interpolar_alfa(canal.alfa_ini, canal.alfa_fin, progreso)

                l_nueva.text = string.format("{%s\\pos(%g,%g)\\fs%g\\b1\\3a&HFF&\\4a&HFF&\\c%s\\1a%s}%s",
                    tags_originales, x_pos, y_pos, fs_estilo, color_actual, alfa_actual, texto_limpio)

                table.insert(lineas_nuevas, l_nueva)
            end

            current_start = current_end
            frame_index = frame_index + 1
        end

        -- FASE 2: Medio estático
        if tiempo_entrada_fin < tiempo_salida_ini then
            local l_normal = util.copy(line)
            l_normal.start_time = tiempo_entrada_fin
            l_normal.end_time = tiempo_salida_ini
            l_normal.effect = "Glitch"
            l_normal.layer = 3
            l_normal.extra = extra_independiente(line.extra, "_aegi_folddata")
            l_normal.text = string.format("{%s\\pos(%g,%g)\\fs%g\\b1}%s", tags_originales, centro_x, y_base, fs_estilo, texto_limpio)
            table.insert(lineas_nuevas, l_normal)
        end

        -- FASE 3: Salida
        current_start = tiempo_salida_ini
        frame_index = 0

        if modo_aber then generar_aberracion(tiempo_salida_ini, total_end) end

        while not modo_aber and current_start < total_end do
            local current_end = current_start + frame_duration
            if current_end > total_end then current_end = total_end end

            local aplicar_desvio = (frame_index ~= 0 and frame_index ~= 3)

            for _, canal in ipairs(canales) do
                local l_nueva = util.copy(line)
                l_nueva.start_time = current_start
                l_nueva.end_time = current_end
                l_nueva.effect = "Glitch"
                l_nueva.layer = canal.capa_z
                l_nueva.extra = extra_independiente(line.extra, "_aegi_folddata")

                local x_pos = aplicar_desvio and (centro_x + (desv_x * canal.mult_x)) or centro_x
                local y_pos = aplicar_desvio and (y_base + (desv_y * canal.mult_y)) or y_base
                local alfa_actual = "&H" .. canal.alfa_fin .. "&"

                l_nueva.text = string.format("{%s\\pos(%g,%g)\\fs%g\\b1\\3a&HFF&\\4a&HFF&\\c%s\\1a%s}%s",
                    tags_originales, x_pos, y_pos, fs_estilo, canal.color_fin, alfa_actual, texto_limpio)

                table.insert(lineas_nuevas, l_nueva)
            end
            current_start = current_end
            frame_index = frame_index + 1
        end

        -- CARPETA ARCH1T3CT (cierre)
        local f_end = util.copy(line)
        f_end.layer = 0
        f_end.comment = true
        f_end.effect = "EndFolder"
        f_end.text = ""
        f_end.extra = { _aegi_folddata = "1;1;1" }
        table.insert(lineas_nuevas, f_end)

        -- Reemplazar en Aegisub
        subtitles[index] = lineas_nuevas[1]
        for j = 2, #lineas_nuevas do
            subtitles.insert(index + j - 1, lineas_nuevas[j])
        end
    end

    aegisub.set_undo_point(script_name)
end

aegisub.register_macro(script_name, script_description, glitch_secuencial)