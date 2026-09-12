# Satellite colour strips

Every strip in this directory was built by `tools/satmaps/build_satmaps.py`
from public satellite data. Colour comes from Sentinel-2 cloudless imagery
by EOX IT Services GmbH (https://s2maps.eu), contains modified Copernicus
Sentinel data 2020, licensed CC BY 4.0. Elevation ordering comes from the
Terrarium tiles of the AWS Open Data terrain set (Mapzen, SRTM, ETOPO1 and
others; see https://github.com/tilezen/joerd/blob/master/docs/attribution.md).

| Library | Strip | Region | Ordering |
| --- | --- | --- | --- |
| rock | 00_atacama_andes | Andean flank above the Atacama, Chile (-23.35, -67.75) | elevation |
| rock | 01_death_valley | Panamint Range and Death Valley, USA (36.25, -117.05) | elevation |
| rock | 02_grand_canyon | Grand Canyon, USA (36.15, -112.15) | elevation |
| rock | 03_zagros | Zagros folds, Iran (29.85, 52.45) | elevation |
| rock | 04_hajar | Al Hajar mountains, Oman (23.20, 57.25) | elevation |
| rock | 05_sinai | Southern Sinai, Egypt (28.55, 33.95) | elevation |
| rock | 06_hoggar | Hoggar massif, Algeria (23.30, 5.55) | elevation |
| rock | 07_canyonlands | Canyonlands, Utah, USA (38.30, -109.90) | elevation |
| rock | 08_tibesti | Tibesti mountains, Chad (20.90, 17.05) | elevation |
| rock | 09_ladakh | Ladakh, India (34.15, 77.55) | elevation |
| green | 00_bernese_alps | Bernese Oberland, Switzerland (46.60, 7.95) | elevation |
| green | 01_appalachians | Blue Ridge, USA (36.40, -81.60) | elevation |
| green | 02_highlands | Scottish Highlands, UK (56.95, -4.65) | elevation |
| green | 03_fjords | Sognefjord, Norway (61.35, 6.85) | elevation |
| green | 04_carpathians | Eastern Carpathians, Romania (47.45, 25.05) | elevation |
| green | 05_pyrenees | Central Pyrenees, Spain and France (42.70, 0.55) | elevation |
| green | 06_tasmania | Central Highlands, Tasmania (-41.95, 146.30) | elevation |
| green | 07_cascades | North Cascades, USA (48.55, -121.05) | elevation |
| green | 08_dolomites | Dolomites, Italy (46.50, 11.85) | elevation |
| green | 09_hokkaido | Daisetsuzan, Japan (43.60, 142.85) | elevation |
| sand | 00_erg_chebbi | Erg Chebbi, Morocco (31.15, -3.95) | elevation |
| sand | 01_namib | Namib sand sea, Namibia (-24.75, 15.40) | elevation |
| sand | 02_rub_al_khali | Rub al Khali, Saudi Arabia (20.50, 52.00) | elevation |
| sand | 03_taklamakan | Taklamakan, China (39.00, 83.00) | elevation |
| sand | 04_simpson | Simpson Desert, Australia (-25.00, 137.00) | elevation |
| sand | 05_white_sands | White Sands, USA (32.80, -106.30) | elevation |
| sand | 06_gobi | Gobi, Mongolia (43.60, 103.90) | elevation |
| sand | 07_sonoran | Algodones dunes, USA (32.90, -115.05) | elevation |
| sand | 08_grand_erg | Grand Erg Oriental, Algeria (30.80, 7.40) | elevation |
| sand | 09_wadi_rum | Wadi Rum, Jordan (29.60, 35.40) | elevation |
| snow | 00_mont_blanc | Mont Blanc massif, France and Italy (45.85, 6.90) | elevation |
| snow | 01_karakoram | Baltoro glacier, Pakistan (35.75, 76.45) | elevation |
| snow | 02_everest | Khumbu, Nepal (27.95, 86.85) | elevation |
| snow | 03_alaska_range | Denali, USA (63.05, -151.00) | elevation |
| snow | 04_patagonia | Southern Patagonian Icefield, Chile (-49.30, -73.30) | elevation |
| snow | 05_skaftafell | Skaftafell outlet glaciers, Iceland (64.05, -16.95) | elevation |
| snow | 06_columbia_icefield | Columbia Icefield, Canada (52.20, -117.30) | elevation |
| snow | 07_svalbard | Spitsbergen, Svalbard (78.20, 16.00) | elevation |
| snow | 08_southern_alps | Aoraki, New Zealand (-43.60, 170.20) | elevation |
| snow | 09_greenland_edge | Ice sheet margin near Kangerlussuaq, Greenland (67.10, -50.05) | elevation |
| blue | 00_bahamas | Exuma banks, Bahamas (24.00, -76.60) | luminance |
| blue | 01_barrier_reef | Great Barrier Reef, Australia (-16.60, 145.95) | luminance |
| blue | 02_maldives | Ari atoll, Maldives (4.00, 72.90) | luminance |
| blue | 03_red_sea | Red Sea coast, Egypt (27.30, 33.85) | luminance |
| blue | 04_lake_powell | Lake Powell, USA (37.05, -111.10) | luminance |
| blue | 05_bora_bora | Society Islands, French Polynesia (-16.50, -151.75) | luminance |
| blue | 06_dalmatia | Dalmatian coast, Croatia (43.15, 16.75) | luminance |
| blue | 07_tahoe | Lake Tahoe, USA (39.10, -120.05) | luminance |
| blue | 08_lofoten | Lofoten, Norway (68.20, 14.00) | luminance |
| blue | 09_andaman | Andaman coast, Thailand (8.00, 98.40) | luminance |
| color | 00_painted_desert | Painted Desert, USA (35.10, -109.80) | elevation |
| color | 01_danakil | Danakil depression, Ethiopia (14.20, 40.30) | elevation |
| color | 02_landmannalaugar | Landmannalaugar, Iceland (63.98, -19.06) | elevation |
| color | 03_zhangye | Zhangye Danxia, China (38.95, 100.10) | elevation |
| color | 04_vinicunca | Vinicunca, Peru (-13.87, -71.30) | elevation |
| color | 05_kamchatka | Klyuchevskaya group, Russia (56.05, 160.60) | elevation |
| color | 06_hawaii | Mauna Loa flank, USA (19.50, -155.50) | elevation |
| color | 07_etna | Mount Etna, Italy (37.75, 15.00) | elevation |
| color | 08_bryce | Bryce Canyon, USA (37.60, -112.20) | elevation |
| color | 09_tenerife | Teide, Spain (28.27, -16.64) | elevation |
