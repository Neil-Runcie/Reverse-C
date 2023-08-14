AUTO_CAP_ON
AUTO_TRIM_ON
MATCH_ALL_ON

ignore k{newline} {\n}
ignore k{space} {[ \t]+}
ignore k{comment} {//.*}

accept k{byte} {[01]\{8\}}
