using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace AppSupervisionPetra
{
    public partial class ConnectForm : Form
    {
        public bool Accepted { get; set; }
        public string IP { get { return this.tboxIP.Text; } }
        public int Port { get { return Int32.Parse(this.tboxPort.Text); } }
        public ConnectForm()
        {
            InitializeComponent();
            Accepted = false;
        }

        // ANNULER
        private void btnCancel_Click(object sender, EventArgs e)
        {
            this.Close();
        }
        // ACCEPTER
        private void btnOK_Click(object sender, EventArgs e)
        {
            Accepted = true;
            this.Close();
        }
    }
}
