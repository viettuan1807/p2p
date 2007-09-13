DROP TABLE IF EXISTS search;
CREATE TABLE search (hash TEXT, name TEXT, size INTEGER, server_IP TEXT, file_ID TEXT);
CREATE UNIQUE INDEX IF NOT EXISTS nameIndex ON search (name);
INSERT INTO search (hash, name, size, server_IP, file_ID)
VALUES ('426CE948672CF42682F23AED2DF7C8F11CE693CD', 'Double Dragon.avi', '23842790', '192.168.1.3,192.168.1.4,192.168.1.5,192.168.1.114', '2,2,2,2');
INSERT INTO search (hash, name, size, server_IP, file_ID)
VALUES ('EF14E1F157601188D4C8B1DC905FC0664B5FBF86', 'Double Dragon 2.avi', '20576554', '192.168.1.3,192.168.1.4,192.168.1.5,192.168.1.114', '4,4,4,4');
INSERT INTO search (hash, name, size, server_IP, file_ID)
VALUES ('05EB36B483CCEAE80DB4852FDEC81981351974B2', 'Excitebike.avi', '21953008', '192.168.1.3,192.168.1.4,192.168.1.5,192.168.1.114', '3,3,3,3');
INSERT INTO search (hash, name, size, server_IP, file_ID)
VALUES ('669D5F2A93B4B2D77BF451ED7C2B658379BFAE4D', 'Super Mario 2 (Jap).avi', '21208890', '192.168.1.3,192.168.1.4,192.168.1.5,192.168.1.114', '1,1,1,1');

