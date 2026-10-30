script_name = "LightX"
script_description = "Piscada inicial personalizada → colores de estilo con guardado, auto brillo y soporte para gradientes"
script_author = "Peteka & Stark (Modificado por Soniz)"
script_version = "4.5"

local json = require("json")
local config_path = aegisub.decode_path("?user/pisteca_config.json")

-- Configuración por defecto
local default_config = {
    tempo = 100,
    custom_brilho = false,
    cor_brilho = "#FFFFFF",
    auto_brilho = false,
    cor1 = "#FF0000",
    cor3 = "#880000",
    cor4 = "#880000",
    intensidade_alfa = 0, -- 0 = 100% Sólido, 255 = 100% Transparente
    autocolor = true,
    auto_gradiente = false
}

-- Cargar configuración previa si existe
local function load_config()
    local file = io.open(config_path, "r")
    if file then
        local content = file:read("*all")
        file:close()
        local status, parsed = pcall(json.decode, content)
        if status and type(parsed) == "table" then
            for k, v in pairs(parsed) do
                default_config[k] = v
            end
        end
    end
end

-- Guardar la configuración actual
local function save_config(config)
    local file = io.open(config_path, "w")
    if file then
        file:write(json.encode(config))
        file:close()
    end
end

-- Función para convertir #RRGGBB a &HBBGGRR&
local function converter_cor(cor)
    if string.sub(cor, 1, 1) == "#" then
        local r = string.sub(cor, 2, 3)
        local g = string.sub(cor, 4, 5)
        local b = string.sub(cor, 6, 7)
        return "&H" .. b .. g .. r .. "&"
    else
        return cor
    end
end

-- Función para extraer el color base del estilo ignorando el alfa
local function cor_do_estilo(cor_estilo)
    local cor = string.match(cor_estilo, "&H%x%x(%x%x%x%x%x%x)")
    if cor then
        return "&H" .. cor .. "&"
    end
    return cor_estilo
end

-- Convertir formato &HBBGGRR& a valores R, G, B
local function ass_to_rgb(ass_color)
    local b, g, r = string.match(ass_color, "&H(%x%x)(%x%x)(%x%x)&")
    if not b then return 255, 255, 255 end -- Blanco por defecto si falla
    return tonumber(r, 16), tonumber(g, 16), tonumber(b, 16)
end

-- Convertir valores R, G, B a formato &HBBGGRR&
local function rgb_to_ass(r, g, b)
    return string.format("&H%02X%02X%02X&", b, g, r)
end

-- Función para mezclar un color ASS con blanco
-- factor_blanco: 0.0 (color original) a 1.0 (blanco puro)
local function aclarar_color(ass_color, factor_blanco)
    local r, g, b = ass_to_rgb(ass_color)
    
    local blend_r = math.floor(r + (255 - r) * factor_blanco)
    local blend_g = math.floor(g + (255 - g) * factor_blanco)
    local blend_b = math.floor(b + (255 - b) * factor_blanco)
    
    return rgb_to_ass(blend_r, blend_g, blend_b)
end

function piscada(subs, sel)
    load_config() -- Cargamos los últimos datos guardados

    local styles = {}
    for i = 1, #subs do
        if subs[i].class == "style" then
            styles[subs[i].name] = subs[i]
        elseif subs[i].class == "dialogue" then
            break
        end
    end

    -- Reestructuración de la interfaz (GUI)
    local dialog_config = {
        {class="label", label="Tiempo del destello (ms):", x=0, y=0},
        {class="intedit", name="tempo", value=default_config.tempo, min=50, max=10000, x=0, y=1},

        {class="checkbox", name="custom_brilho", label="Usar color de brillo manual:", value=default_config.custom_brilho, x=0, y=2},
        {class="color", name="cor_brilho", value=default_config.cor_brilho, x=1, y=2},

        {class="checkbox", name="auto_brilho", label="Auto Brillo (Aclara el color final del texto mezclándolo con blanco)", value=default_config.auto_brilho, x=0, y=3, width=2},

        {class="label", label="Transparencia Inicial (0=Sólido, 255=Invis):", x=0, y=4},
        {class="intedit", name="intensidade_alfa", value=default_config.intensidade_alfa, min=0, max=255, x=1, y=4},

        {class="label", label="Color Primario (\\1c):", x=0, y=5},
        {class="color", name="cor1", value=default_config.cor1, x=0, y=6},

        {class="label", label="Color Borde (\\3c):", x=1, y=5},
        {class="color", name="cor3", value=default_config.cor3, x=1, y=6},

        {class="label", label="Color Sombra (\\4c):", x=0, y=7},
        {class="color", name="cor4", value=default_config.cor4, x=0, y=8},
        
        {class="checkbox", name="autocolor", label="Auto Color (Usar colores del estilo al finalizar en lugar de los de arriba)", value=default_config.autocolor, x=0, y=9, width=2},
        
        {class="checkbox", name="auto_gradiente", label="Auto Gradient (Soporte para líneas con degradados/colores por letra)", value=default_config.auto_gradiente, x=0, y=10, width=2}
    }

    local pressed, results = aegisub.dialog.display(dialog_config, {"Aplicar", "Guardar y Aplicar", "Cancelar"})
    
    if not pressed or pressed == "Cancelar" then 
        aegisub.cancel() 
    end

    if pressed == "Guardar y Aplicar" then
        save_config(results)
    end

    local cor_brilho_manual = converter_cor(results.cor_brilho)
    local alfa_inicial = string.format("&H%02X&", results.intensidade_alfa)

    local cor1_default = converter_cor(results.cor1)
    local cor3_default = converter_cor(results.cor3)
    local cor4_default = converter_cor(results.cor4)

    for _, i in ipairs(sel) do
        local line = subs[i]
        local texto_original = line.text

        local c1 = cor1_default
        local c3 = cor3_default
        local c4 = cor4_default

        if results.autocolor and styles[line.style] then
            c1 = cor_do_estilo(styles[line.style].color1)
            c3 = cor_do_estilo(styles[line.style].color3)
            c4 = cor_do_estilo(styles[line.style].color4)
        end

        if results.auto_gradiente then
            local nuevo_texto = ""
            
            -- Si el texto no empieza con una etiqueta, manejamos el texto inicial plano
            if not string.match(texto_original, "^{") then
                local texto_inicial = string.match(texto_original, "^([^{]+)")
                if texto_inicial then
                    local cor_brilho_inicio = results.auto_brilho and aclarar_color(c1, 0.75) or (results.custom_brilho and cor_brilho_manual or "&HFFFFFF&")
                    nuevo_texto = nuevo_texto .. string.format("{\\1c%s\\3c%s\\4c%s\\alpha%s\\t(0,%d,\\1c%s\\3c%s\\4c%s\\alpha&H00&)}%s",
                        cor_brilho_inicio, cor_brilho_inicio, cor_brilho_inicio, alfa_inicial, results.tempo, c1, c3, c4, texto_inicial)
                end
            end

            -- Procesamos cada bloque de {etiquetas}texto
            for tags, texto_bloque in string.gmatch(texto_original, "({[^}]*})([^{]*)") do
                -- Extraer colores específicos del bloque actual si existen, si no, heredar los anteriores conocidos
                local c1_bloque = string.match(tags, "\\[1]?c&H(%x+)&") and ("&H" .. string.match(tags, "\\[1]?c&H(%x+)&") .. "&") or c1
                local c3_bloque = string.match(tags, "\\3c&H(%x+)&") and ("&H" .. string.match(tags, "\\3c&H(%x+)&") .. "&") or c3
                local c4_bloque = string.match(tags, "\\4c&H(%x+)&") and ("&H" .. string.match(tags, "\\4c&H(%x+)&") .. "&") or c4

                -- Actualizar los colores base para mantener la herencia secuencial del ASS
                c1, c3, c4 = c1_bloque, c3_bloque, c4_bloque

                -- Determinar brillo inicial para este segmento específico
                local cor_brilho_inicio
                if results.auto_brilho then
                    cor_brilho_inicio = aclarar_color(c1_bloque, 0.75)
                elseif results.custom_brilho then
                    cor_brilho_inicio = cor_brilho_manual
                else
                    cor_brilho_inicio = "&HFFFFFF&" 
                end

                -- Limpiar etiquetas viejas de color y alfa del bloque original para evitar conflictos internos
                local tags_limpias = string.gsub(tags, "\\[1]?c&H%x+&", "")
                tags_limpias = string.gsub(tags_limpias, "\\3c&H%x+&", "")
                tags_limpias = string.gsub(tags_limpias, "\\4c&H%x+&", "")
                tags_limpias = string.gsub(tags_limpias, "\\alpha&H%x+&", "")
                tags_limpias = string.gsub(tags_limpias, "[{}]", "") -- Quitar llaves temporales

                -- Construir bloque animado manteniendo los tags de efectos nativos (\fs, \t, etc.) intactos
                nuevo_texto = nuevo_texto .. string.format(
                    "{\\1c%s\\2c%s\\3c%s\\4c%s\\alpha%s%s\\t(0,%d,\\1c%s\\3c%s\\4c%s\\alpha&H00&)}%s",
                    cor_brilho_inicio, cor_brilho_inicio, cor_brilho_inicio, cor_brilho_inicio, alfa_inicial,
                    tags_limpias ~= "" and ("\\" .. tags_limpias) or "",
                    results.tempo,
                    c1_bloque, c3_bloque, c4_bloque,
                    texto_bloque
                )
            end
            line.text = nuevo_texto
        else
            -- Comportamiento estándar del script original para líneas sin degradados por letra
            local cor_brilho_inicio
            if results.auto_brilho then
                cor_brilho_inicio = aclarar_color(c1, 0.75)
            elseif results.custom_brilho then
                cor_brilho_inicio = cor_brilho_manual
            else
                cor_brilho_inicio = "&HFFFFFF&" 
            end

            line.text = string.format(
                "{\\1c%s\\3c%s\\4c%s\\alpha%s" ..
                "\\t(0,%d,\\1c%s\\3c%s\\4c%s\\alpha&H00&)}%s",
                cor_brilho_inicio, cor_brilho_inicio, cor_brilho_inicio, alfa_inicial,
                results.tempo,
                c1, c3, c4,
                texto_original
            )
        end

        subs[i] = line
    end

    aegisub.set_undo_point("Piscada Custom Correta")
end

aegisub.register_macro("Piscada Custom Correta", "Piscada color/brillo → colores en patrón ASS", piscada)