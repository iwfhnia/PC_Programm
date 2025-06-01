//Einbinden benötigter Bibliotheken
#include <windows.h>      // Windows API für serielle Kommunikation und systemnahe Funktionen
#include <iostream>       // Standard Ein-/Ausgabe (cout, cin, cerr)
#include <fstream>        // Datei-Ein-/Ausgabe (logging)
#include <string>         // std::string für einfache Zeichenkettenverwaltung
#include <algorithm>      // std::remove_if zum Entfernen von Zeichen (hier Leerzeichen)

// Prüft, ob der gegebene Ausdruck gültig ist:
// - Er muss genau einen Operator (+, -, *, /) enthalten
// - Beide Operanden müssen im Bereich 0 bis 1000 liegen
// - Wenn gültig, wird der gefundene Operator zurückgegeben
bool isValidExpression(const std::string& expr, char& op) {
    size_t pos = expr.find_first_of("+-*/"); // Suche ersten Operator im String
    if (pos == std::string::npos)            // Falls kein Operator gefunden:
        return false;                        // Ausdruck ungültig

    try {
        int z1 = std::stoi(expr.substr(0, pos));     // Erster Operant: String bis Operator konvertieren
        int z2 = std::stoi(expr.substr(pos + 1));    // Zweiter Operant: String nach Operator konvertieren

        if (z1 < 0 || z1 > 1000 || z2 < 0 || z2 > 1000) // Prüfe Bereich der Zahlen
            return false;                                // Operanden außerhalb des Wertebereichs → gibt ´false´ zurück

        op = expr[pos];                                 // Operator in Variable óp´ speichern
        return true;                                    // Ausdruck gültig
    }
    catch (...) {                                        // Ausnahmen werden abgefangen (Fehler bei Konvertierung)
        return false;                                   // gibt ´false´ zurück, wenn eine Ausnahme auftritt
    }
}

int main() {                          //Starten des Hauptprogramms
    HANDLE hSerial = CreateFileA(     //Öffnen des seriellen Ports:
        "\\\\.\\COM5",               // COM-Port COM5 im Windows-Format (Arduino-Schnittstelle)
        GENERIC_READ | GENERIC_WRITE, // Lese- und Schreibzugriff auf den geöffneten Port
        0,                          // Kein Sharing erlaubt (exklusiver Zugriff)
        NULL,                       // Keine speziellen Sicherheitsattribute
        OPEN_EXISTING,              // Nur existierenden Port öffnen
        0,                          // Keine besonderen Flags oder Attribute
        NULL);                      // Kein Template-Handle

    if (hSerial == INVALID_HANDLE_VALUE) {   // Prüfen, ob der Port geöffnet werden konnte
        std::cerr << "COM-Port konnte nicht geöffnet werden.\n"; // Fehlermeldung, wenn nicht erfolgreich
        return 1;                           // Programm mit Fehlercode beenden
    }

    DCB dcb = { sizeof(DCB) };       // Device Control Block für serielle Schnittstelle initialisieren 
    GetCommState(hSerial, &dcb);     // Aktuelle Einstellungen des Ports lesen
    dcb.BaudRate = CBR_9600;         // Baudrate 9600 Baud setzen (analog zu Arduino)
    dcb.ByteSize = 8;                // 8 Datenbits
    dcb.StopBits = ONESTOPBIT;       // 1 Stoppbit (markiert Ende des Datenpakets)
    dcb.Parity = NOPARITY;           // Keine Paritätsprüfung
    SetCommState(hSerial, &dcb);     // Neue Einstellungen übernehmen

    Sleep(2000);                     // 2 Sekunden warten, damit Arduino initialisieren kann

    std::ofstream logfile("rechnungen.txt", std::ios::app); // Log-Datei im Anhang-Modus öffnen

    std::string eingabe;             // Variable für die Benutzereingabe (Rechenaufgabe)
    std::string antwort;             // Variable für die Antwort vom Arduino
    char op;                        // Variable zum Speichern des Operators

    while (true) {                  // Endlosschleife für wiederholte Benutzereingaben
        std::cout << "\nGib eine Rechnung ein (z.B. 12+34)\n"
            << "Tippe 'exit' zum Beenden\n> ";  // Eingabeaufforderung ausgeben
        std::getline(std::cin, eingabe);              // Ganze Zeile vom Benutzer einlesen (inkl. Leerzeichen) und in `eingabe` speichern

        // Entferne alle Leerzeichen aus der Eingabe (z.B. "12 + 34" wird zu "12+34")
        eingabe.erase(std::remove_if(eingabe.begin(), eingabe.end(), isspace), eingabe.end());

        if (eingabe == "exit")  // Prüfen, ob der Benutzer das Programm beenden möchte
            break;              // Bei Eingabe von ´exit´: Schleife verlassen, Programm beenden

        if (!isValidExpression(eingabe, op)) {  // Prüfe, ob die Eingabe gültig ist
            std::cerr << "Ungültige Eingabe.\n";       // Fehlerrückmeldung bei ungültiger Eingabe
            logfile << "Eingabe: " << eingabe << " => Ergebnis: ungültige Eingabe\n"; // Eingabe und Fehlermeldung in Logdatei schreiben
            continue;                     // Zur nächsten Eingabe springen (Schleife fortsetzen)
        }

        eingabe.push_back('\n');         // Zeilenumbruch an die Eingabe anhängen (als Trennzeichen für Arduino)
        DWORD written;                   // Variable für Anzahl geschriebener Bytes
        WriteFile(hSerial, eingabe.c_str(), eingabe.size(), &written, NULL); // Ausdruck an Arduino senden
        Sleep(100);                     // Kurz warten, bis Arduino antwortet

        antwort.clear();                // Antwort-String leeren für neue Daten
        char c;                        // Variable für gelesene Zeichen
        DWORD bytesRead;               // Anzahl gelesener Bytes

        // Zeichenweise Antwort lesen, bis Zeilenumbruch empfangen wird
        while (ReadFile(hSerial, &c, 1, &bytesRead, NULL) && bytesRead > 0 && c != '\n') {
            antwort.push_back(c);      // Zeichen an Antwort-String anhängen
        }

        std::cout << "Ergebnis: " << antwort << "\n";   // Ergebnis auf der Konsole ausgeben

        // Prüfen, ob Antwort mit "Fehler:" beginnt (Fehlermeldung Arduino)
        if (antwort.rfind("Fehler:", 0) == 0) {
            std::cerr << "(vom Arduino): " << antwort << "\n";  // Wenn ja: Fehlermeldung ausgeben
            logfile << "Eingabe: " << std::string(eingabe.c_str(), eingabe.size() - 1)
                << " => Ergebnis: " << antwort << "\n";    // Eingabe und Fehler im Log vermerken
        }
        else {                          // Liegt keine Fehlermeldung vor:
            logfile << "Eingabe: " << std::string(eingabe.c_str(), eingabe.size() - 1)
                << " => Antwort: " << antwort << "\n";    // Eingabe und berechnetes Ergebnis in Logdatei schreiben
        }
    }

    CloseHandle(hSerial);    // Seriellen Port schließen (Ressourcen freigeben)
    std::cout << "\nProgramm beendet.\n";  // Abschlussmeldung ausgeben
    return 0;               // Programm erfolgreich beenden
}
