script_name = "Remove Tags"
script_description = "Dynamically remove only the tags that YTSubConverter reads (no dependencies)"
script_author = "Based on PhosCity's Remove Tags"
script_version = "2.0.0"


-- Tags read by YTSubConverter (https://github.com/arcusmaximus/YTSubConverter, section "ASS feature support"):
--   \b \i \u \fn \fs \c \1c \2c \3c \4c \1a \2a \3a \4a \alpha \pos \an \k \r \fad \fade \move \t
-- Internal keys used by this script (\c and \1c share the key "c1"):
local tagOrder = {
    "b", "i", "u", "fn", "fs",
    "c1", "c2", "c3", "c4",
    "alpha", "a1", "a2", "a3", "a4",
    "pos", "an", "k", "r",
    "fad", "fade", "move", "t",
}

local tagLabels = {
    b = "\\b", i = "\\i", u = "\\u", fn = "\\fn", fs = "\\fs",
    c1 = "\\1c & \\c", c2 = "\\2c", c3 = "\\3c", c4 = "\\4c",
    alpha = "\\alpha", a1 = "\\1a", a2 = "\\2a", a3 = "\\3a", a4 = "\\4a",
    pos = "\\pos", an = "\\an", k = "\\k", r = "\\r",
    fad = "\\fad", fade = "\\fade", move = "\\move", t = "\\t",
}

local groupDefinitions = {
    {key = "color",    label = "color",    tags = {"c1", "c2", "c3", "c4"},         hint = "c, 1c, 2c, 3c, 4c"},
    {key = "alpha",    label = "alpha",    tags = {"alpha", "a1", "a2", "a3", "a4"}, hint = "alpha, 1a, 2a, 3a, 4a"},
    {key = "font",     label = "font",     tags = {"fn", "fs"},                      hint = "fn, fs"},
    {key = "style",    label = "style",    tags = {"b", "i", "u"},                   hint = "b, i, u"},
    {key = "fade",     label = "fade",     tags = {"fad", "fade"},                   hint = "fad, fade"},
    {key = "position", label = "position", tags = {"pos", "move", "an"},             hint = "pos, move, an"},
}

local hints = {
    start_tag = "Remove from start tags only",
    inline_tags = "Remove from inline tags only",
    transforms = "Remove from inside \\t only",
    invert = "Remove all except selected",
    inline_except_last = "Remove all inline tags except the last one",
}


---------------------------------------------------------------------------
-- Parsing
---------------------------------------------------------------------------

-- Does the tag text `s` (without the leading backslash) start with `name`
-- and is the name not just the beginning of a longer tag (e.g. \fs vs \fscx)?
local function hasName(s, name)
    if s:sub(1, #name) ~= name then return false end
    local nxt = s:sub(#name + 1, #name + 1)
    return nxt == "" or not nxt:match("%a")
end

-- Returns the key of a tag segment such as "\fs20" if YTSubConverter reads it, nil otherwise.
local function classify(seg)
    local s = seg:sub(2)
    if s:sub(1, 2) == "fn" then return "fn" end
    if s:sub(1, 1) == "r" then
        if s:match("^rnd[xyz]?[%-%d%.]") then return nil end
        return "r" -- \r and \rStyleName
    end
    if hasName(s, "alpha") then return "alpha" end
    if hasName(s, "fade") then return "fade" end
    if hasName(s, "fad") then return "fad" end
    if hasName(s, "move") then return "move" end
    if hasName(s, "pos") then return "pos" end
    if hasName(s, "fs") then return "fs" end
    if hasName(s, "an") then return "an" end
    if hasName(s, "1c") or hasName(s, "c") then return "c1" end
    if hasName(s, "2c") then return "c2" end
    if hasName(s, "3c") then return "c3" end
    if hasName(s, "4c") then return "c4" end
    if hasName(s, "1a") then return "a1" end
    if hasName(s, "2a") then return "a2" end
    if hasName(s, "3a") then return "a3" end
    if hasName(s, "4a") then return "a4" end
    if hasName(s, "b") then return "b" end
    if hasName(s, "i") then return "i" end
    if hasName(s, "u") then return "u" end
    if hasName(s, "k") then return "k" end
    if hasName(s, "t") then return "t" end
    return nil
end

-- Splits the content of an override block into the text before the first tag
-- (a comment) and a list of tag segments. A new segment starts at every backslash
-- that is not inside parentheses, so \t(0,100,\c&HFF&) stays one segment.
local function splitBlock(content)
    local segs, depth, start = {}, 0, nil
    local leading = ""
    for i = 1, #content do
        local ch = content:sub(i, i)
        if ch == "(" then
            depth = depth + 1
        elseif ch == ")" then
            if depth > 0 then depth = depth - 1 end
        elseif ch == "\\" and depth == 0 then
            if start then
                segs[#segs + 1] = content:sub(start, i - 1)
            else
                leading = content:sub(1, i - 1)
            end
            start = i
        end
    end
    if start then
        segs[#segs + 1] = content:sub(start)
    else
        leading = content
    end
    return leading, segs
end

local function matchParen(str, open)
    local depth = 0
    for i = open, #str do
        local ch = str:sub(i, i)
        if ch == "(" then
            depth = depth + 1
        elseif ch == ")" then
            depth = depth - 1
            if depth == 0 then return i end
        end
    end
    return nil
end

-- Splits a \t(...) segment into: text before the inner tags, list of inner tag segments, text after.
-- Returns nil if the transform has no inner tags.
local function splitTransform(seg)
    local open = seg:find("(", 1, true)
    if not open then return nil end
    local innerStart = seg:find("\\", open + 1, true)
    if not innerStart then return nil end
    local close = matchParen(seg, open)
    local innerEnd = close and (close - 1) or #seg
    local _, innerSegs = splitBlock(seg:sub(innerStart, innerEnd))
    return seg:sub(1, innerStart - 1), innerSegs, close and seg:sub(close) or ""
end

-- Splits a line into text pieces and override blocks ({...}).
local function parseLine(text)
    local pieces, pos = {}, 1
    while true do
        local s, e = text:find("{[^}]*}", pos)
        if not s then break end
        if s > pos then
            pieces[#pieces + 1] = {kind = "text", value = text:sub(pos, s - 1)}
        end
        pieces[#pieces + 1] = {kind = "block", value = text:sub(s + 1, e - 1)}
        pos = e + 1
    end
    if pos <= #text then
        pieces[#pieces + 1] = {kind = "text", value = text:sub(pos)}
    end

    -- A block with at least one backslash is a tag block (the others are comments).
    -- Tag blocks before any text are "start" tags, the ones after text are "inline" tags.
    local seenText = false
    for _, p in ipairs(pieces) do
        if p.kind == "text" then
            seenText = true
        elseif p.value:find("\\", 1, true) then
            p.isTag = true
            p.isStart = not seenText
        end
    end
    return pieces
end


---------------------------------------------------------------------------
-- Collecting what is present in the selection
---------------------------------------------------------------------------

local function collectLine(text, info)
    local inlineCount = 0
    for _, p in ipairs(parseLine(text)) do
        if p.isTag then
            if p.isStart then
                info.hasStart = true
            else
                inlineCount = inlineCount + 1
            end

            local _, segs = splitBlock(p.value)
            for _, seg in ipairs(segs) do
                local key = classify(seg)
                if key then
                    info.keys[key] = true
                    if key == "t" then
                        info.hasTransforms = true
                        local _, innerSegs = splitTransform(seg)
                        for _, innerSeg in ipairs(innerSegs or {}) do
                            local innerKey = classify(innerSeg)
                            if innerKey and innerKey ~= "t" then info.keys[innerKey] = true end
                        end
                    end
                end
            end
        end
    end
    if inlineCount > 0 then info.hasInline = true end
    if inlineCount >= 2 then info.multipleInline = true end
end


---------------------------------------------------------------------------
-- Removing
---------------------------------------------------------------------------

-- Handles one \t(...) segment when only the tags inside transforms should be removed.
-- Returns the new segment, or nil if nothing is left inside it.
local function processTransform(seg, remove)
    local prefix, innerSegs, suffix = splitTransform(seg)
    if not prefix then return seg end

    local kept, removedAny = {}, false
    for _, innerSeg in ipairs(innerSegs) do
        local key = classify(innerSeg)
        if key and key ~= "t" and remove(key) then
            removedAny = true
        else
            kept[#kept + 1] = innerSeg
        end
    end
    if not removedAny then return seg end
    if #kept == 0 then return nil end
    return prefix .. table.concat(kept) .. suffix
end

-- transformsOnly = false: act on the tags of the block itself (\t is removed as a whole).
-- transformsOnly = true: act only on the tags inside \t(...).
local function processBlock(content, remove, transformsOnly)
    local leading, segs = splitBlock(content)
    local out = {leading}
    for _, seg in ipairs(segs) do
        local key = classify(seg)
        local keep = seg
        if transformsOnly then
            if key == "t" then keep = processTransform(seg, remove) end
        elseif key and remove(key) then
            keep = nil
        end
        if keep then out[#out + 1] = keep end
    end
    return table.concat(out)
end

-- cfg.scope: "all" | "start" | "inline" | "inline_except_last"
local function applyToLine(text, cfg)
    local pieces = parseLine(text)

    local lastInline
    for _, p in ipairs(pieces) do
        if p.isTag and not p.isStart then lastInline = p end
    end

    for _, p in ipairs(pieces) do
        if p.isTag then
            local target = cfg.scope == "all"
                or (cfg.scope == "start" and p.isStart)
                or (cfg.scope == "inline" and not p.isStart)
                or (cfg.scope == "inline_except_last" and not p.isStart and p ~= lastInline)
            if target then
                p.value = processBlock(p.value, cfg.remove, cfg.transformsOnly)
            end
        end
    end

    local out = {}
    for _, p in ipairs(pieces) do
        if p.kind == "text" then
            out[#out + 1] = p.value
        else
            out[#out + 1] = "{" .. p.value .. "}"
        end
    end
    local result = table.concat(out)
    result = result:gsub("{}", "")
    return result
end


---------------------------------------------------------------------------
-- Macro
---------------------------------------------------------------------------

local function showError(message)
    aegisub.dialog.display({{class = "label", label = message}}, {"Close"}, {cancel = "Close"})
    aegisub.cancel()
end

local function main(subs, sel)

    local indexes = {}
    for _, idx in ipairs(sel) do
        if subs[idx].class == "dialogue" then indexes[#indexes + 1] = idx end
    end
    if #indexes == 0 then return end

    local info = {keys = {}}
    for _, idx in ipairs(indexes) do
        collectLine(subs[idx].text, info)
    end

    local tagList = {}
    for _, key in ipairs(tagOrder) do
        if info.keys[key] then tagList[#tagList + 1] = key end
    end
    if #tagList == 0 then
        showError("No tags read by YTSubConverter were found in the selected lines.")
    end

    local groups = {}
    for _, group in ipairs(groupDefinitions) do
        for _, key in ipairs(group.tags) do
            if info.keys[key] then
                groups[#groups + 1] = group
                break
            end
        end
    end

    -- Dialog
    local buttons = {"Remove Tags", "Remove All", "Cancel"}
    local dialog, y = {}, 0

    for _, group in ipairs(groups) do
        dialog[#dialog + 1] = {x = 0, y = y, class = "checkbox", label = "Remove all " .. group.label,
            name = "group_" .. group.key, hint = group.hint}
        y = y + 1
    end
    if info.multipleInline then
        dialog[#dialog + 1] = {x = 0, y = y, class = "checkbox", label = "Remove all inline except last",
            name = "group_inline_except_last", hint = hints.inline_except_last}
        y = y + 1
    end

    local startX = 0
    if #dialog > 0 then
        table.insert(buttons, 1, "Remove Group")
        startX = 1
    end
    local x = startX

    local options = {
        {name = "start_tag", label = "Start tag", show = info.hasStart},
        {name = "inline_tags", label = "Inline tags", show = info.hasInline},
        {name = "transforms", label = "Transforms", show = info.hasTransforms},
        {name = "invert", label = "Invert", show = true},
    }
    for _, option in ipairs(options) do
        if option.show then
            dialog[#dialog + 1] = {x = x, y = 0, class = "checkbox", label = option.label,
                name = option.name, hint = hints[option.name]}
            x = x + 1
        end
    end

    local column = math.max(math.ceil(math.sqrt(#tagList)), x)
    local count = 0
    for row = 1, column do
        for col = startX, column + startX - 1 do
            count = count + 1
            if count > #tagList then break end
            local key = tagList[count]
            dialog[#dialog + 1] = {x = col, y = row, class = "checkbox", label = tagLabels[key], name = "tag_" .. key}
        end
    end

    local btn, res = aegisub.dialog.display(dialog, buttons, {cancel = "Cancel"})
    if not btn or btn == "Cancel" then aegisub.cancel() end

    -- Work out what to remove and where
    local removeSet = {}
    local cfg = {scope = "all", transformsOnly = false}

    if res.start_tag then
        cfg.scope = "start"
    elseif res.inline_tags then
        cfg.scope = "inline"
    end

    if btn == "Remove All" then
        for _, key in ipairs(tagOrder) do removeSet[key] = true end

    elseif btn == "Remove Group" then
        if res.group_inline_except_last then
            cfg.scope = "inline_except_last"
            for _, key in ipairs(tagOrder) do removeSet[key] = true end
        else
            for _, group in ipairs(groups) do
                if res["group_" .. group.key] then
                    for _, key in ipairs(group.tags) do removeSet[key] = true end
                end
            end
            if res.invert then
                local inverted = {}
                for _, key in ipairs(tagOrder) do
                    if not removeSet[key] then inverted[key] = true end
                end
                removeSet = inverted
            end
        end

    elseif btn == "Remove Tags" then
        for _, key in ipairs(tagList) do
            local checked = res["tag_" .. key] and true or false
            local inverted = res.invert and true or false
            if checked ~= inverted then removeSet[key] = true end
        end
        if res.transforms and not res.start_tag and not res.inline_tags then
            cfg.transformsOnly = true
        end
    end

    cfg.remove = function(key) return removeSet[key] == true end

    -- Apply
    for n, idx in ipairs(indexes) do
        if aegisub.progress.is_cancelled() then aegisub.cancel() end
        aegisub.progress.title("Removing tags")
        aegisub.progress.task("Processing line " .. n .. "/" .. #indexes)
        aegisub.progress.set(100 * n / #indexes)

        local line = subs[idx]
        local newText = applyToLine(line.text, cfg)
        if newText ~= line.text then
            line.text = newText
            subs[idx] = line
        end
    end

    if aegisub.set_undo_point then aegisub.set_undo_point(script_name) end
    return sel
end

aegisub.register_macro(script_name, script_description, main)