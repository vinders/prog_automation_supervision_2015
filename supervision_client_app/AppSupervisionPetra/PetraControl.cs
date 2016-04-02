using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace AppSupervisionPetra
{
    public partial class PetraControl : UserControl
    {
        public PetraControl()
        {
            InitializeComponent();
        }

        // GESTION DES BLOCS ------------------------------------------------------
        #region Propriétés actuateurs
        public Label blockA01a { get { return A01a; } set { A01a = value; } }
        public Label blockA01b { get { return A01b; } set { A01b = value; } }
        public Label blockA01c { get { return A01c; } set { A01c = value; } }
        public Label blockA01d { get { return A01d; } set { A01d = value; } }
        public Label blockA2 { get { return A2; } set { A2 = value; } }
        public Label blockA3 { get { return A3; } set { A3 = value; } }
        public Label blockA4a { get { return A4a; } set { A4a = value; } }
        public Label blockA4b { get { return A4b; } set { A4b = value; } }
        public Label blockA4c { get { return A4c; } set { A4c = value; } }
        public Label blockA4d { get { return A4d; } set { A4d = value; } }
        public Label blockA5a { get { return A5a; } set { A5a = value; } }
        public Label blockA5b { get { return A5b; } set { A5b = value; } }
        public Label blockA5c { get { return A5c; } set { A5c = value; } }
        public Label blockA5d { get { return A5d; } set { A5d = value; } }
        public Label blockA6a { get { return A6a; } set { A6a = value; } }
        public Label blockA6b { get { return A6b; } set { A6b = value; } }
        public Label blockA7a { get { return A7a; } set { A7a = value; } }
        public Label blockA7b { get { return A7b; } set { A7b = value; } }
        #endregion
        #region Propriétés capteurs
        public Label blockS0 { get { return S0; } set { S0 = value; } }
        public Label blockS1 { get { return S1; } set { S1 = value; } }
        public Label blockS2a { get { return S2a; } set { S2a = value; } }
        public Label blockS2b { get { return S2b; } set { S2b = value; } }
        public Label blockS3 { get { return S3; } set { S3 = value; } }
        public Label blockS4 { get { return S4; } set { S4 = value; } }
        public Label blockS5 { get { return S5; } set { S5 = value; } }
        public Label blockS6a { get { return S6a; } set { S6a = value; } }
        public Label blockS6b { get { return S6b; } set { S6b = value; } }
        public Label blockS6c { get { return S6c; } set { S6c = value; } }
        public Label blockS6d { get { return S6d; } set { S6d = value; } }
        public Label blockS7 { get { return S7; } set { S7 = value; } }
        #endregion

        #region handlers blocs
        private void A01a_Click(object sender, EventArgs e)
        {
            ((SupervisionForm)Parent).PositionValue = 0;
            ((SupervisionForm)Parent).numA01_ValueChanged(sender, e);
        }
        private void A01b_Click(object sender, EventArgs e)
        {
            ((SupervisionForm)Parent).PositionValue = 1;
            ((SupervisionForm)Parent).numA01_ValueChanged(sender, e);
        }
        private void A01c_Click(object sender, EventArgs e)
        {
            ((SupervisionForm)Parent).PositionValue = 2;
            ((SupervisionForm)Parent).numA01_ValueChanged(sender, e);
        }
        private void A01d_Click(object sender, EventArgs e)
        {
            ((SupervisionForm)Parent).PositionValue = 3;
            ((SupervisionForm)Parent).numA01_ValueChanged(sender, e);
        }

        private void A2_Click(object sender, EventArgs e)
        {
            ((SupervisionForm)Parent).btnA2_Click(sender, e);
        }
        private void A3_Click(object sender, EventArgs e)
        {
            ((SupervisionForm)Parent).btnA3_Click(sender, e);
        }
        private void A4_Click(object sender, EventArgs e)
        {
            ((SupervisionForm)Parent).btnA4_Click(sender, e);
        }
        private void A5_Click(object sender, EventArgs e)
        {
            ((SupervisionForm)Parent).btnA5_Click(sender, e);
        }
        private void A6_Click(object sender, EventArgs e)
        {
            ((SupervisionForm)Parent).btnA6_Click(sender, e);
        }
        private void A7_Click(object sender, EventArgs e)
        {
            ((SupervisionForm)Parent).btnA7_Click(sender, e);
        }
        #endregion
    }
}
