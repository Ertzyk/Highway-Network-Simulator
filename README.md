# Rozproszony Symulator Sieci Autostrad

Zaawansowany, wieloprocesowy i wielowątkowy symulator ruchu drogowego stworzony w języku C z wykorzystaniem niskopoziomowych mechanizmów systemu operacyjnego (POSIX / System V IPC). Projekt realizuje dynamiczne trasowanie pojazdów w czasie rzeczywistym, reagując na asynchroniczne zdarzenia drogowe (wypadki/korki) w środowisku współbieżnym.

## Architektura Systemu

System opiera się na rozproszonej architekturze, w której różne elementy infrastruktury drogowej działają jako niezależne procesy systemu operacyjnego:

* **Globalny Dyspozytor (Główny Proces):** Odpowiada za cykl życia systemu. Wczytuje konfigurację grafu drogowego, inicjalizuje struktury IPC (Pamięć Współdzielona, Kolejki Komunikatów), powołuje do życia procesy węzłów za pomocą `fork()` oraz wstrzykuje nowe pojazdy do systemu operacyjnego.
* **Węzły / Skrzyżowania (Procesy Potomne):** Każde skrzyżowanie to w pełni niezależny proces nasłuchujący na dedykowanej Kolejce Komunikatów. Każdy proces posiada wewnętrzną **Pulę Wątków (Thread Pool)** zrealizowaną za pomocą biblioteki `pthreads`, co pozwala na równoległą obsługę wielu pojazdów jednocześnie.
* **Stan Współdzielony:** Mapa drogowa (wagi krawędzi reprezentujące czas przejazdu) przechowywana jest w Pamięci Współdzielonej, do której dostęp chroniony jest przez semafory POSIX, aby zapobiec zjawisku wyścigu (Race Condition).

## Główne Funkcjonalności i Mechanizmy OS

* **Komunikacja Międzyprocesowa (IPC - Message Queues):** Pojazdy podróżują między procesami-skrzyżowaniami za pośrednictwem systemowych kolejek komunikatów (System V `msgget` / `msgsnd` / `msgrcv`).
* **Pamięć Współdzielona i Semafory (Shared Memory & Semaphores):** Stan zakorkowania dróg znajduje się w pamięci (`shmget`), a każda modyfikacja (np. dodanie auta na trasę) jest chroniona globalnym semaforem.
* **Wielowątkowość i Synchronizacja (Pthreads, Mutexes, Condition Variables):** Skrzyżowania wykorzystują wzorzec *Producer-Consumer*. Główny wątek procesu odbiera komunikaty IPC i przekazuje je do lokalnej kolejki zadań chronionej Mutexem. Zmienne warunkowe (`pthread_cond_t`) służą do efektywnego budzenia śpiących wątków roboczych bez zbędnego obciążania procesora (brak *busy-waiting*).
* **Asynchroniczne Sygnały i Reaktywność (`SIGUSR1`, `SIGINT`):** Dyspozytor symuluje losowe zdarzenia drogowe (wypadki), wysyłając sygnał `SIGUSR1` do wybranych węzłów. Węzeł modyfikuje wtedy pamięć współdzieloną. Sygnał `SIGINT` (Ctrl+C) jest przechwytywany w celu bezpiecznego i całkowitego zwolnienia zasobów IPC (zapobieganie wyciekom pamięci i procesom zombie).
* **Biblioteki Dynamiczne (Dynamic Libraries):** Moduł odpowiedzialny za algorytmikę (wyznaczanie najkrótszej trasy algorytmem Dijkstry) został skompilowany jako biblioteka dynamiczna (`librouting.so`). Odczytuje ona aktualne (zmieniające się) wagi grafu bezpośrednio z pamięci współdzielonej, co umożliwia pojazdom dynamiczne znajdowanie objazdów w przypadku wypadków.

## Struktura Katalogów

* `src/` - Kod źródłowy w języku C (Główny dyspozytor, logika węzłów, algorytm trasowania).
* `include/` - Pliki nagłówkowe definiujące wspólny protokół IPC (struktury wiadomości, układ pamięci współdzielonej).
* `lib/` - Katalog docelowy dla biblioteki dynamicznej `librouting.so`.
* `bin/` - Katalog docelowy dla skompilowanego programu wykonawczego.
* `config/` - Pliki tekstowe z konfiguracją początkową grafu dróg.

## Kompilacja i Uruchomienie

Projekt wykorzystuje plik `Makefile` do automatyzacji procesu budowania.

1. Zbuduj projekt (kompilacja biblioteki dynamicznej oraz głównego programu wykonawczego):
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
