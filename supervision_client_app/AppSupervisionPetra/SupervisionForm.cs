using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Net.Sockets;
using System.Threading;

namespace AppSupervisionPetra
{
    public partial class SupervisionForm : Form
    {
        #region Membres
        private TcpClient _connection = null;
        private Thread _threadConnection = null;
        private NetworkStream _stream = null;
        private static Object _mutexLock = new Object();
        private bool[] _actuators;
        private bool[] _sensors;
        #endregion
        #region Propriétés
        public bool IsConnected { get; set; }
        public bool[] Sensors { get { return _sensors; } set { _sensors = value; } }
        public bool[] Actuators { get { return _actuators; } set { _actuators = value; } }
        public int PositionValue { set { numA01.Value = value; } }
        #endregion

        // INITIALISATION -------------------------------------------------------------------
        public SupervisionForm()
        {
            InitializeComponent();
            IsConnected = false;
            // état E/S
            Actuators = new bool[8];
            Sensors = new bool[8];
            for (int i = 0; i < 8; i++)
            {
                Actuators[i] = false;
                Sensors[i] = false;
            }
            // affichage
            petraControl.blockA6b.Visible = false;
            petraControl.blockA7b.Visible = false;
            petraControl.blockA4b.Visible = false;
            petraControl.blockA4c.Visible = false;
            petraControl.blockA4d.Visible = false;
            petraControl.blockA5b.Visible = false;
            petraControl.blockA5c.Visible = false;
            petraControl.blockA5d.Visible = false;
            petraControl.blockS6b.Visible = false;
            petraControl.blockS6c.Visible = false;
            petraControl.blockS6d.Visible = false;
        }

        #region Entrées-sorties
        // ETABLIR CONNEXION ----------------------------------------------------------------
        private void btnConnect_Click(object sender, EventArgs e)
        {
            lock (_mutexLock)
            {
                if (IsConnected == false) // connexion
                {
                    // connexion TCP
                    try
                    {
                        // boîte de dialogue
                        ConnectForm dlg = new ConnectForm();
                        dlg.ShowDialog();
                        // établir connexion
                        if (dlg.Accepted)
                        {
                            _connection = new TcpClient();
                            _connection.Connect(dlg.IP, dlg.Port);
                            _stream = _connection.GetStream();
                            this.btnConnect.Text = "Déconnexion";
                            IsConnected = true;
                        }
                    }
                    catch (Exception exc) // erreur
                    {
                        IsConnected = false;
                        _connection = null;
                        MessageBox.Show("Connexion impossible : " + exc.ToString(), "Echec de connexion",
                                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    // thread de réception
                    try
                    {
                        _threadConnection = new Thread(new ThreadStart(this.ReceiveSensorsState));
                        _threadConnection.Start();
                    }
                    catch (Exception exc) // erreur
                    {
                        _threadConnection = null;
                        MessageBox.Show("Erreur de lancement du thread de réception : " + exc.ToString(), "Crash de thread",
                                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    // mise à jour des actuateurs
                    UpdateActuators();
                }
                else // déconnexion
                {
                    // fin thread
                    try
                    {
                        _threadConnection.Abort();
                    }
                    catch (Exception exc)
                    {
                        MessageBox.Show("Erreur de fin de thread : " + exc.ToString());
                    }
                    _threadConnection = null;
                    // déconnexion TCP
                    try
                    {
                        _stream.Close();
                    }
                    catch (Exception exc)
                    {
                        MessageBox.Show("Erreur de fermeture de flux : " + exc.ToString());
                    }
                    _stream = null;
                    try
                    {
                        _connection.Close();
                    }
                    catch (Exception exc)
                    {
                        MessageBox.Show("Erreur de déconnexion : " + exc.ToString());
                    }
                    _connection = null;
                    this.btnConnect.Text = "Connexion...";
                    IsConnected = false;
                }
            }
        }

        // REQUETE ECRITURE -----------------------------------------------------------------
        public void SendActuatorsState(byte val)
        {
            lock (_mutexLock)
            {
                byte[] sentBlock = { val, 1, 1, 0 }; // 4 octets
                try
                {
                    _stream.Write(sentBlock, 0, sentBlock.Length); // écriture
                }
                catch (Exception exc)
                {
                    MessageBox.Show("Erreur d'écriture : " + exc.ToString());
                }
            }
        }
        // RECEPTION LECTURE ----------------------------------------------------------------
        protected void ReceiveSensorsState()
        {
            byte[] sensorsVal = new byte[4]; // 4 octets
            bool success = true;
            while (true)
            {
                try
                {
                    _stream.Read(sensorsVal, 0, 4); // lecture
                    success = true;
                }
                catch (Exception exc)
                {
                    success = false;
                    MessageBox.Show("Erreur d'écriture : " + exc.ToString());
                }
                if (success)
                    UpdateSensors((int)sensorsVal[0]); // affichage
                Thread.Sleep(100);
            }
        }
        // MISE A JOUR - ECRITURE DES ACTUATEURS --------------------------------------------
        public void UpdateActuators()
        {
            if (IsConnected)
            {
                // calcul de valeur à écrire
                int actuatorsVal = 0;
                int mask = 0x1;
                for (int i = 0; i < 8; i++)
                {
                    if (Actuators[i])
                        actuatorsVal |= mask;
                    mask *= 2;
                }
                // requete
                SendActuatorsState((byte)actuatorsVal);
            }
            this.Refresh();
        }
        // MISE A JOUR - AFFICHAGE DES CAPTEURS ---------------------------------------------
        public void UpdateSensors(int val)
        {
            // séparation des valeurs
            int mask = 0x1;
            for (int i = 0; i < 8; i++)
            {
                if ((val & mask) != 0)
                    Sensors[i] = true;
                else
                    Sensors[i] = false;
                mask *= 2;
            }
            // affichage
            if (Sensors[0])
            {
                stateS0.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockS0.BackColor = Color.DarkSeaGreen;
            }
            else
            {
                stateS0.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockS0.BackColor = Color.Silver;
            }
            if (Sensors[1])
            {
                stateS1.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockS1.BackColor = Color.DarkSeaGreen;
            }
            else
            {
                stateS1.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockS1.BackColor = Color.Silver;
            }
            if (Sensors[2])
            {
                stateS2.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockS2a.BackColor = Color.DarkSeaGreen;
                petraControl.blockS2b.BackColor = Color.DarkSeaGreen;
            }
            else
            {
                stateS2.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockS2a.BackColor = Color.Silver;
                petraControl.blockS2b.BackColor = Color.Silver;
            }
            if (Sensors[3])
            {
                stateS3.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockS3.BackColor = Color.DarkSeaGreen;
            }
            else
            {
                stateS3.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockS3.BackColor = Color.Silver;
            }
            if (Sensors[4])
            {
                stateS4.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockS4.BackColor = Color.DarkSeaGreen;
            }
            else
            {
                stateS4.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockS4.BackColor = Color.Silver;
            }
            if (Sensors[5])
            {
                stateS5.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockS5.BackColor = Color.DarkSeaGreen;
            }
            else
            {
                stateS5.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockS5.BackColor = Color.Silver;
            }
            if (Sensors[6])
            {
                stateS6.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockS6a.BackColor = Color.DarkSeaGreen;
                petraControl.blockS6b.BackColor = Color.DarkSeaGreen;
                petraControl.blockS6c.BackColor = Color.DarkSeaGreen;
                petraControl.blockS6d.BackColor = Color.DarkSeaGreen;
            }
            else
            {
                stateS6.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockS6a.BackColor = Color.Silver;
                petraControl.blockS6b.BackColor = Color.Silver;
                petraControl.blockS6c.BackColor = Color.Silver;
                petraControl.blockS6d.BackColor = Color.Silver;
            }
            if (Sensors[7])
            {
                stateS7.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockS7.BackColor = Color.DarkSeaGreen;
            }
            else
            {
                stateS7.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockS7.BackColor = Color.Silver;
            }
        }
        #endregion

        #region handlers boutons
        public void numA01_ValueChanged(object sender, EventArgs e) // position CP
        {
            Actuators[0] = (((int)numA01.Value & 0x1) != 0);
            Actuators[1] = (((int)numA01.Value & 0x2) != 0);
            // affichage témoin
            stateA01.Text = numA01.Value.ToString();
            // affichage graphique
            petraControl.blockA4a.Visible = false;
            petraControl.blockA4b.Visible = false;
            petraControl.blockA4c.Visible = false;
            petraControl.blockA4d.Visible = false;
            petraControl.blockA5a.Visible = false;
            petraControl.blockA5b.Visible = false;
            petraControl.blockA5c.Visible = false;
            petraControl.blockA5d.Visible = false;
            petraControl.blockS6a.Visible = false;
            petraControl.blockS6b.Visible = false;
            petraControl.blockS6c.Visible = false;
            petraControl.blockS6d.Visible = false;
            if (Actuators[1] == false)
            {
                if (Actuators[0] == false) // 0
                {
                    petraControl.blockA01a.ForeColor = Color.Green;
                    petraControl.blockA01b.ForeColor = Color.Red;
                    petraControl.blockA4a.Visible = true;
                    petraControl.blockA5a.Visible = true;
                    petraControl.blockS6a.Visible = true;
                }
                else // 1
                {
                    petraControl.blockA01a.ForeColor = Color.Red;
                    petraControl.blockA01b.ForeColor = Color.Green;
                    petraControl.blockA4b.Visible = true;
                    petraControl.blockA5b.Visible = true;
                    petraControl.blockS6b.Visible = true;
                }
                petraControl.blockA01c.ForeColor = Color.Red;
                petraControl.blockA01d.ForeColor = Color.Red;
            }
            else
            {
                petraControl.blockA01a.ForeColor = Color.Red;
                petraControl.blockA01b.ForeColor = Color.Red;
                if (Actuators[0] == false) // 2
                {
                    petraControl.blockA01c.ForeColor = Color.Green;
                    petraControl.blockA01d.ForeColor = Color.Red;
                    petraControl.blockA4c.Visible = true;
                    petraControl.blockA5c.Visible = true;
                    petraControl.blockS6c.Visible = true;
                }
                else // 3
                {
                    petraControl.blockA01c.ForeColor = Color.Red;
                    petraControl.blockA01d.ForeColor = Color.Green;
                    petraControl.blockA4d.Visible = true;
                    petraControl.blockA5d.Visible = true;
                    petraControl.blockS6d.Visible = true;
                }
            }
            UpdateActuators();
        }
        public void btnA2_Click(object sender, EventArgs e) // convoyeur C1
        {
            Actuators[2] = !Actuators[2];
            // affichage
            if (Actuators[2])
            {
                stateA2.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockA2.ForeColor = Color.Green;
            }
            else
            {
                stateA2.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockA2.ForeColor = Color.Red;
            }
            UpdateActuators();
        }
        public void btnA3_Click(object sender, EventArgs e) // convoyeur C2
        {
            Actuators[3] = !Actuators[3];
            if (Actuators[3])
            {
                stateA3.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockA3.ForeColor = Color.Green;
            }
            else
            {
                stateA3.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockA3.ForeColor = Color.Red;
            }
            UpdateActuators();
        }
        public void btnA4_Click(object sender, EventArgs e) // PV : aspiration
        {
            Actuators[4] = !Actuators[4];
            if (Actuators[4])
            {
                stateA4.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockA4a.ForeColor = Color.Green;
                petraControl.blockA4b.ForeColor = Color.Green;
                petraControl.blockA4c.ForeColor = Color.Green;
                petraControl.blockA4d.ForeColor = Color.Green;
            }
            else
            {
                stateA4.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockA4a.ForeColor = Color.Red;
                petraControl.blockA4b.ForeColor = Color.Red;
                petraControl.blockA4c.ForeColor = Color.Red;
                petraControl.blockA4d.ForeColor = Color.Red;
            }
            UpdateActuators();
        }
        public void btnA5_Click(object sender, EventArgs e) // PA : plongeur
        {
            Actuators[5] = !Actuators[5];
            if (Actuators[5])
            {
                stateA5.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockA5a.ForeColor = Color.Green;
                petraControl.blockA5b.ForeColor = Color.Green;
                petraControl.blockA5c.ForeColor = Color.Green;
                petraControl.blockA5d.ForeColor = Color.Green;
            }
            else
            {
                stateA5.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockA5a.ForeColor = Color.Red;
                petraControl.blockA5b.ForeColor = Color.Red;
                petraControl.blockA5c.ForeColor = Color.Red;
                petraControl.blockA5d.ForeColor = Color.Red;
            }
            UpdateActuators();
        }
        public void btnA6_Click(object sender, EventArgs e) // AA : arbre
        {
            Actuators[6] = !Actuators[6];
            if (Actuators[6])
            {
                stateA6.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockA7a.Visible = false;
                petraControl.blockA6b.Visible = true;
                petraControl.blockA7b.Visible = true;
                petraControl.blockA6a.Visible = false;
            }
            else
            {
                stateA6.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockA7b.Visible = false;
                petraControl.blockA6a.Visible = true;
                petraControl.blockA7a.Visible = true;
                petraControl.blockA6b.Visible = false;
            }
            UpdateActuators();
        }
        public void btnA7_Click(object sender, EventArgs e) // GA : grappin
        {
            Actuators[7] = !Actuators[7];
            if (Actuators[7])
            {
                stateA7.Image = Image.FromFile("../../Resources/led_on.png");
                petraControl.blockA7a.ForeColor = Color.Green;
                petraControl.blockA7b.ForeColor = Color.Green;
            }
            else
            {
                stateA7.Image = Image.FromFile("../../Resources/led_off.png");
                petraControl.blockA7a.ForeColor = Color.Red;
                petraControl.blockA7b.ForeColor = Color.Red;
            }
            UpdateActuators();
        }
        #endregion
    }
}
