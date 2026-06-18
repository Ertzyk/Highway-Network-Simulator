# Rozproszony Symulator Sieci Autostrad

Projekt symuluje ruch drogowy w oparciu o architekturę wieloprocesową i wielowątkową. System dynamicznie reaguje na zmiany przepustowości dróg w czasie rzeczywistym, wykorzystując mechanizmy komunikacji międzyprocesowej (IPC) zgodne ze standardem POSIX i System V.

## Architektura i Wykorzystane Mechanizmy

System został zaprojektowany tak, aby każdy element infrastruktury działał niezależnie, z naciskiem na brak aktywnego oczekiwania (busy-waiting) i bezpieczną modyfikację danych w środowisku współbieżnym.

1. **Zarządzanie procesami (fork):**
   Główny program (Dyspozytor) wczytuje topologię sieci i powołuje do życia niezależny proces potomny dla każdego skrzyżowania (węzła). System operacyjny zarządza nimi równolegle, a Dyspozytor zachowuje ich numery PID do późniejszego zarządzania cyklem życia.

2. **Komunikacja międzyprocesowa (Kolejki Komunikatów IPC):**
   Procesy węzłów komunikują się asynchronicznie za pomocą systemowych kolejek komunikatów (`msgget`, `msgsnd`, `msgrcv`). Przemieszczający się pojazd to struktura danych przesyłana przez IPC. Każdy węzeł nasłuchuje wyłącznie na wiadomości zaadresowane do jego identyfikatora.

3. **Globalna mapa drogowa (Pamięć Współdzielona):**
   Macierz czasów przejazdu (wagi grafu) rezyduje we fragmencie pamięci RAM przypiętym do przestrzeni adresowej wszystkich procesów (`shmget`, `shmat`). Dzięki temu każde skrzyżowanie posiada w czasie rzeczywistym identyczny widok na stan zakorkowania sieci.

4. **Mechanizm wzajemnego wykluczania (Semafory POSIX):**
   Aby zapobiec zjawisku wyścigu (race condition) podczas jednoczesnej modyfikacji mapy dróg przez wiele procesów, dostęp do pamięci współdzielonej jest chroniony globalnym semaforem (`sem_t`). Zapewnia to integralność danych podczas symulacji ruchu.

5. **Równoległa obsługa pojazdów (Pula Wątków i Zmienne Warunkowe):**
   Wewnątrz każdego procesu węzła zaimplementowano wzorzec Producer-Consumer z użyciem biblioteki `pthreads`. Zamiast aktywnie obciążać procesor (busy-waiting), wątki robocze są usypiane przez zmienne warunkowe (`pthread_cond_t`). Gdy do węzła dociera nowy pojazd z IPC, główny wątek umieszcza go w kolejce lokalnej chronionej Mutexem (`pthread_mutex_t`) i wybudza jeden z wątków roboczych w celu wytyczenia trasy.

6. **Wstrzykiwanie zdarzeń (Sygnały SIGUSR1 i SIGINT):**
   Dyspozytor co pewien czas wysyła sygnał `SIGUSR1` do losowo wybranego procesu potomnego. Handler tego sygnału modyfikuje pamięć współdzieloną (drastycznie zwiększając wagę jednej z dróg), co symuluje nagły wypadek drogowy. Sygnał `SIGINT` (Ctrl+C) odpowiada natomiast za *Graceful Shutdown* – łapie polecenie przerwania, bezpiecznie terminuje procesy potomne i niszczy w systemie operacyjnym struktury IPC, zapobiegając wyciekom pamięci.

7. **Dynamiczne przeliczanie tras (Biblioteki Współdzielone .so):**
   Algorytm Dijkstry odpowiedzialny za wyznaczanie optymalnej ścieżki został skompilowany jako osobna biblioteka dynamiczna (`librouting.so`). Ponieważ algorytm w czasie rzeczywistym operuje na pamięci współdzielonej, pojazdy automatycznie modyfikują swoje trasy, omijając drogi zablokowane przez sygnały alarmowe.

## Struktura Katalogów

* `src/` - Kod źródłowy w języku C (Główny dyspozytor, logika węzłów, algorytm trasowania).
* `include/` - Pliki nagłówkowe definiujące wspólny protokół IPC (struktury wiadomości, układ pamięci współdzielonej).
* `lib/` - Katalog docelowy dla biblioteki dynamicznej `librouting.so`.
* `bin/` - Katalog docelowy dla skompilowanego programu wykonawczego.
* `config/` - Pliki tekstowe z konfiguracją początkową grafu dróg.

## Kompilacja i Uruchomienie

1. Budowanie projektu (kompilacja biblioteki dynamicznej oraz kodu źródłowego):
   ```bash
   make
   ```
2. Uruchom symulator:
    ```bash
    ./bin/dispatcher
    ```
3. Aby bezpiecznie zakończyć działanie systemu (zabicie procesów potomnych i wyczyszczenie pamięci IPC), wciśnij Ctrl + C. System automatycznie przechwyci sygnał i przeprowadzi procedurę Graceful Shutdown.
4. Aby wyczyścić pliki binarne:
    ```bash
    make clean
    ```
