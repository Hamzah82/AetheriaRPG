# Pelita di Atas Segara Garam

**Gema Garam & Abu: Hikayat Laut yang Membeku**

RPG 2D berbasis giliran (*turn-based*) dengan eksplorasi peta, party empat orang,
sistem memori dan resonansi emosi, serta empat ending. Semua sprite, musik, dan
efek suara dibuat secara prosedural; tidak ada aset eksternal.

Tagline: *"Setiap kenangan yang kau bakar untuk menerangi jalan, akan meninggalkan
dingin di dalam dadamu."*

## Cerita

Di kepulauan **Nusakalpa**, lautan telah mengering menjadi hamparan garam dan langit
diselimuti abu. Ingatan manusia adalah bahan bakar terakhir: *Garam Kenangan*
disuling menjadi *Minyak Sumsum* untuk menyalakan lentera yang menahan Wabah Pualam.

**Raka Damar**, seorang *Juru Gema*, menemukan cetak biru mesin raksasa dari seorang
kurir yang membatu. Bersama **Sekar Wijayanti**, **Bima Prakasa**, **Larasati**, dan
**Kakek Bagas**, ia mendorong konflik dari Desa Tambak-Abu menuju Pelabuhan Besi
Kertarajasa, Sumur Ratapan, Biara Ordo Pelita Putih, Kraton Pualam Surya, dan akhirnya
Jantung Palung Sunyi.

Ada empat act utama:

1. **Kota Seribu Lentera** — membentuk aliansi dan menemukan bukti monopoli minyak.
2. **Bisikan di Bawah Garam** — membuktikan penyebab Wabah Pualam dan bertemu arwah ayah Raka.
3. **Harga Sebuah Pelita** — konfrontasi dengan Bunda Nirmala, Kirana, dan Adipati Mahardika.
4. **Requiem Segara Garam** — menghentikan Bahtera Langit dan menentukan nasib Jangkar Gema.

## Sistem Permainan

### Party dan pertarungan

- Empat rekan otomatis bertindak sesuai peran: Sekar (DPS), Bima (tank), Larasati
  (penyembuh), dan Bagas (debuffer/pegang ritme). Raka dikendalikan pemain.
- Perintah Raka: **Serang**, **Gema** (sihir), **Item**, dan **Kabur**.
- Skill Gema: *Api Garam*, *Petir Garam*, *Ledakan Kenangan*, *Kidung Garam*, dan
  *Tumbal Rasa*.
- Musuh memiliki tipe lemah api/petir, bos memiliki fase kedua, dan beberapa status
  dikendalikan Bagas.

### Membaca Gema dan memori

- Kristal bercahaya di peta menyimpan 12 fragmen memori. Sentuh untuk memutar
  *echoscape* dan menambahkannya ke jurnal.
- Memori membuka lore, misi tersembunyi, dan syarat ending.

### Resonansi emosi

- **Resonansi** Raka menurun saat memakai skill Gema dan memilih dialog dingin.
- Resonansi rendah mengunci opsi empatik dan membuka opsi dingin.
- Api unggun memulihkan HP/MP party, menghabiskan 1 Minyak Sumsum, dan menaikkan
  resonansi.
- Resonansi menentukan akhir cerita.

### Minyak, garam, dan crafting

- **Keping Garam** adalah mata uang; **Minyak Sumsum** untuk api unggun; **Garam
  Kenangan** untuk pengrajin.
- Toko Joko Keling menjual ramuan, eliksir, minyak, senjata, dan armor.

### Ending

Empat akhir: **True Ending** (resonansi tinggi, semua karakter bergabung, misi
tersembunyi, dan 10+ memori), **Good Ending**, **Bad Ending**, dan **Secret Ending**
(Gajamina Pualam + 12 memori + Inti Tirta Purba).

## Kontrol

| Tombol | Fungsi |
| :--- | :--- |
| Panah | Bergerak / memilih |
| `Z` / `Enter` | Interaksi, konfirmasi |
| `X` / `Esc` | Menu / batal |
| `Shift` | Berlari |
| `E` / `Tab` | Menu utama |
| `F11` / `Alt+Enter` | Layar penuh |

## Peta

| ID | Nama |
| :--- | :--- |
| 0 | Desa Tambak-Abu |
| 1 | Pelabuhan Besi Kertarajasa |
| 2 | Rimba Karang-Kelabu |
| 3 | Sumur Ratapan |
| 4 | Candi Tenggelam Saptatirta |
| 5 | Biara Ordo Pelita Putih |
| 6 | Taman Lentera Mati |
| 7 | Kraton Pualam Surya |
| 8 | Jantung Palung Sunyi |

Portal penting disegel berdasarkan act, sehingga cerita tidak bisa dilompati sembarangan.

## Simpan dan Muat

- Simpanan ditulis ke `~/.aetheria/` (Windows: `%USERPROFILE%\.aetheria`).
- Slot: `savegame.dat` (Slot 1), `savegame-2.dat`, `savegame-3.dat`, dan
  `savegame-auto.dat` (autosave).
- Berkas simpanan **dikodekan dan dilindungi checksum**; suntingan manual akan ditolak.
  Simpanan lama versi 1/2 tetap dapat dimuat dan otomatis ditulis ulang dalam format
  terkode saat dimuat.
- Penyimpanan atomik melalui berkas sementara; aman di Linux dan Windows.

## Instalasi

```bash
./install.sh
```

Untuk melewatkan pemasangan dependensi:

```bash
./install.sh --no-deps
```

Opsi lain:

- `--yes` — jangan tanya saat memasang dependensi.
- `--build-only` — build tanpa memasang.
- `--uninstall` — hapus instalasi.
- `--repo URL` — repositori sumber khusus.
- `--clean` — bersihkan hasil build sebelum menyusun.

Kebutuhan: kompiler C++17, `make`, `git`, X11 di Linux, atau MSYS2/MinGW di Windows.

## Menjalankan

```bash
./rpgcpp                # jendela
./rpgcpp --no-audio     # tanpa suara
./rpgcpp --headless     # uji asap
./rpgcpp --screenshot DIR
./rpgcpp --save PATH    # lokasi simpanan khusus
./rpgcpp --settings PATH
./rpgcpp --seed N
```

## Pengembangan

| Berkas | Fungsi |
| :--- | :--- |
| `src/core.h` | tipe dasar, RNG, input, palet |
| `src/world.h/.cpp` | peta, portal, NPC, peti, gema, api unggun |
| `src/player.h/.cpp` | statistik, inventaris, senjata, armor |
| `src/party.h/.cpp` | rekan party dan peran tempur |
| `src/party.h/.cpp` | rekan party dan peran tempur |
| `src/battle.h/.cpp` | pertarungan party berbasis giliran |
| `src/game.h/.cpp` | mesin state, cerita, misi, simpan/muat |
| `src/game_render.cpp` | semua layar |
| `src/art.cpp` | sprite prosedural |
| `src/audio_synth.cpp` | musik dan SFX prosedural |
| `src/savegame.cpp` | slot, metadata, enkode simpanan |
| `src/platform.cpp` / `src/platform_win32.cpp` | X11 dan Win32/GDI |
| `tests/test_main.cpp` | tes deterministik |

```bash
make            # build
make test       # tes
make shots      # tangkuan layar ke shots/
```

Cross-build Windows dari Linux:

```bash
make BUILD=build-win \
  CXX=/tmp/opencode/llvm-mingw/bin/x86_64-w64-mingw32-clang++ \
  WINDRES=/tmp/opencode/llvm-mingw/bin/x86_64-w64-mingw32-windres
```

## Pengujian

`make test` menjalankan 400+ pemeriksaan deterministik: dunia dan portal, simpanan
terkode dan migrasi legacy, resonansi, memori, party battle, act progression, alur
cerita lengkap, render, audio, dan pengaturan.

## Pemecahan Masalah

- **Tidak ada suara**: periksa perangkat audio; gunakan `--no-audio` untuk mode senyap.
- **Simpanan ditolak**: berkas telah diubah atau rusak; pulihkan dari cadangan.
- **Portal menolak**: act saat ini belum cukup; selesaikan misi utama.
- **Resonansi rendah**: beristirahat di api unggun dengan Minyak Sumsum.
